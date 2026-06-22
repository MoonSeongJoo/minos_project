#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "hasee_frame_packet.h"

#define GUARDIAN_IP   "10.10.10.1"
#define GUARDIAN_PORT 7777

#define IMAGE_W 1600
#define IMAGE_H 900

static uint8_t class_to_id(const char *cls)
{
    if (strcmp(cls, "Car") == 0) return HASEE_CLS_CAR;
    if (strcmp(cls, "Pedestrian") == 0) return HASEE_CLS_PEDESTRIAN;
    if (strcmp(cls, "Cone") == 0) return HASEE_CLS_CONE;
    return HASEE_CLS_UNKNOWN;
}

static int load_label(uint32_t frame_id, hasee_frame_packet_t *pkt)
{
    char path[128];
    FILE *f;
    char cls[64];
    float x1, y1, x2, y2, conf;
    int count = 0;

    snprintf(path, sizeof(path), "/demo/labels/frame_%04u.txt", frame_id);

    f = fopen(path, "r");
    if (!f) {
        perror("[VM2 AI] fopen label");
        return -1;
    }

    while (count < HASEE_MAX_OBJS &&
           fscanf(f, "%63s %f %f %f %f %f", cls, &x1, &y1, &x2, &y2, &conf) == 6) {
        pkt->objs[count].x1 = (uint16_t)x1;
        pkt->objs[count].y1 = (uint16_t)y1;
        pkt->objs[count].x2 = (uint16_t)x2;
        pkt->objs[count].y2 = (uint16_t)y2;
        pkt->objs[count].cls = class_to_id(cls);
        pkt->objs[count].conf = conf;
        count++;
    }

    fclose(f);
    pkt->num_objs = count;
    return count;
}

static const char *health_name(uint32_t h)
{
    return h == HASEE_HEALTH_OK ? "OK" : "OOD";
}

int main(void)
{
    int fd;
    struct sockaddr_in dst;
    uint32_t seq = 0;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[VM2 AI] socket");
        return 1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(GUARDIAN_PORT);

    if (inet_aton(GUARDIAN_IP, &dst.sin_addr) == 0) {
        printf("[VM2 AI] invalid guardian ip\n");
        return 1;
    }

    printf("[VM2 AI] nuScenes frame_id demo TX to %s:%d\n", GUARDIAN_IP, GUARDIAN_PORT);
    fflush(stdout);

    while (1) {
        hasee_frame_packet_t pkt;
        uint32_t frame_id = seq % HASEE_NUM_FRAMES;
        int phase = seq % HASEE_NUM_FRAMES;

        memset(&pkt, 0, sizeof(pkt));

        pkt.magic = HASEE_MAGIC;
        pkt.seq = seq;
        pkt.frame_id = frame_id;
        pkt.img_w = IMAGE_W;
        pkt.img_h = IMAGE_H;
        /*
        * 100-frame demo scenario:
        *
        * 0  ~ 29 : NORMAL
        * 30 ~ 39 : OOD / degraded AI state
        * 40 ~ 69 : recovery to NORMAL
        * 70 ~ 79 : latency/health degradation
        * 80 ~ 99 : NORMAL
        */
       if ((phase >= 30 && phase <= 39) ||
            (phase >= 70 && phase <= 79)) {
            pkt.uncertainty = 0.91f;
            pkt.latency_ms = 44;
            pkt.health = HASEE_HEALTH_OOD;
        } else {
            pkt.uncertainty = 0.12f;
            pkt.latency_ms = 23;
            pkt.health = HASEE_HEALTH_OK;
        }

        if (load_label(frame_id, &pkt) < 0) {
            printf("[VM2 AI] failed to load label for frame_id=%u\n", frame_id);
            seq++;
            sleep(1);
            continue;
        }

        if (sendto(fd, &pkt, sizeof(pkt), 0,
                   (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            perror("[VM2 AI] sendto");
        } else {
            printf("[VM2 AI] TX seq=%u frame_id=%u objs=%u uncertainty=%.2f latency_ms=%u health=%s\n",
                   pkt.seq, pkt.frame_id, pkt.num_objs,
                   pkt.uncertainty, pkt.latency_ms, health_name(pkt.health));
            fflush(stdout);
        }

        seq++;
        sleep(1);
    }

    close(fd);
    return 0;
}