#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define GUARDIAN_IP   "10.10.10.1"
#define GUARDIAN_PORT 7777

int main(void)
{
    int fd;
    struct sockaddr_in dst;
    int frame = 0;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[VM2 AI] socket");
        return 1;
    }

    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(GUARDIAN_PORT);

    if (inet_aton(GUARDIAN_IP, &dst.sin_addr) == 0) {
        printf("[VM2 AI] invalid guardian ip: %s\n", GUARDIAN_IP);
        close(fd);
        return 1;
    }

    printf("[VM2 AI] sending metadata to %s:%d\n",
           GUARDIAN_IP, GUARDIAN_PORT);
    fflush(stdout);

    while (1) {
        char msg[256];

        if ((frame % 10) == 9) {
            snprintf(msg, sizeof(msg),
                     "frame=%d uncertainty=0.91 latency_ms=44 health=OOD",
                     frame);
        } else {
            snprintf(msg, sizeof(msg),
                     "frame=%d uncertainty=0.12 latency_ms=23 health=OK",
                     frame);
        }

        if (sendto(fd, msg, strlen(msg), 0,
                   (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            perror("[VM2 AI] sendto");
        } else {
            printf("[VM2 AI] TX %s\n", msg);
            fflush(stdout);
        }

        frame++;
        sleep(1);
    }

    close(fd);
    return 0;
}