#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "hasee_frame_packet.h"

#define DEFAULT_PORT        7788
#define DEFAULT_WARMUP      100
#define DEFAULT_SAMPLES     1000
#define DEFAULT_INTERVAL_US 10000

static uint64_t now_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        perror("clock_gettime");
        exit(1);
    }

    return (uint64_t)ts.tv_sec * 1000000000ULL
         + (uint64_t)ts.tv_nsec;
}

static int compare_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;

    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

static double percentile(
    const double *values,
    size_t count,
    double ratio)
{
    if (count == 0)
        return 0.0;

    size_t index =
        (size_t)((double)(count - 1) * ratio);

    return values[index];
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s DST_IP [PORT] [WARMUP] "
                "[SAMPLES] [INTERVAL_US] [CSV]\n",
                argv[0]);
        return 1;
    }

    const char *dst_ip = argv[1];

    int port = argc >= 3
        ? atoi(argv[2])
        : DEFAULT_PORT;

    int warmup = argc >= 4
        ? atoi(argv[3])
        : DEFAULT_WARMUP;

    int samples = argc >= 5
        ? atoi(argv[4])
        : DEFAULT_SAMPLES;

    int interval_us = argc >= 6
        ? atoi(argv[5])
        : DEFAULT_INTERVAL_US;

    const char *csv_path = argc >= 7
        ? argv[6]
        : "/tmp/udp_rtt.csv";

    if (port <= 0 || port > 65535 ||
        warmup < 0 || samples <= 0 ||
        interval_us < 0) {
        fprintf(stderr, "invalid argument\n");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    if (setsockopt(fd,
                   SOL_SOCKET,
                   SO_RCVTIMEO,
                   &timeout,
                   sizeof(timeout)) != 0) {
        perror("setsockopt");
        close(fd);
        return 1;
    }

    struct sockaddr_in destination;
    memset(&destination, 0, sizeof(destination));

    destination.sin_family = AF_INET;
    destination.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET,
                  dst_ip,
                  &destination.sin_addr) != 1) {
        fprintf(stderr, "invalid IP: %s\n", dst_ip);
        close(fd);
        return 1;
    }

    if (connect(fd,
                (struct sockaddr *)&destination,
                sizeof(destination)) != 0) {
        perror("connect");
        close(fd);
        return 1;
    }

    /*
     * 실제 AI inference metadata packet과 같은 UDP payload 크기.
     */
    unsigned char tx_buffer[sizeof(hasee_frame_packet_t)];
    unsigned char rx_buffer[sizeof(hasee_frame_packet_t)];

    double *rtt_values =
        (double *)calloc((size_t)samples, sizeof(double));

    if (!rtt_values) {
        perror("calloc");
        close(fd);
        return 1;
    }

    FILE *csv = fopen(csv_path, "w");
    if (!csv) {
        perror("fopen CSV");
        free(rtt_values);
        close(fd);
        return 1;
    }

    fprintf(csv, "sample,rtt_us,estimated_one_way_us\n");

    int total = warmup + samples;
    int valid = 0;
    int lost = 0;

    for (int iteration = 0;
         iteration < total;
         iteration++) {

        uint32_t sequence = (uint32_t)iteration;

        memset(tx_buffer, 0xa5, sizeof(tx_buffer));
        memcpy(tx_buffer, &sequence, sizeof(sequence));

        uint64_t start_ns = now_ns();

        ssize_t sent = send(
            fd,
            tx_buffer,
            sizeof(tx_buffer),
            0);

        if (sent != (ssize_t)sizeof(tx_buffer)) {
            fprintf(stderr,
                    "send failed iteration=%d sent=%ld "
                    "errno=%d (%s)\n",
                    iteration,
                    (long)sent,
                    errno,
                    strerror(errno));
            lost++;
            continue;
        }

        ssize_t received = recv(
            fd,
            rx_buffer,
            sizeof(rx_buffer),
            0);

        uint64_t end_ns = now_ns();

        if (received != (ssize_t)sizeof(rx_buffer)) {
            fprintf(stderr,
                    "receive failed iteration=%d received=%ld "
                    "errno=%d (%s)\n",
                    iteration,
                    (long)received,
                    errno,
                    strerror(errno));
            lost++;
            continue;
        }

        uint32_t received_sequence = 0;
        memcpy(&received_sequence,
               rx_buffer,
               sizeof(received_sequence));

        if (received_sequence != sequence) {
            fprintf(stderr,
                    "sequence mismatch tx=%u rx=%u\n",
                    sequence,
                    received_sequence);
            lost++;
            continue;
        }

        double rtt_us =
            (double)(end_ns - start_ns) / 1000.0;

        if (iteration >= warmup) {
            int sample_index = iteration - warmup;

            rtt_values[valid++] = rtt_us;

            fprintf(csv,
                    "%d,%.3f,%.3f\n",
                    sample_index,
                    rtt_us,
                    rtt_us / 2.0);
        }

        if (interval_us > 0)
            usleep((useconds_t)interval_us);
    }

    fclose(csv);

    qsort(rtt_values,
          (size_t)valid,
          sizeof(double),
          compare_double);

    double sum = 0.0;

    for (int i = 0; i < valid; i++)
        sum += rtt_values[i];

    double mean = valid > 0
        ? sum / valid
        : 0.0;

    printf("\n===== UDP RTT RESULT =====\n");
    printf("destination          : %s:%d\n",
           dst_ip, port);
    printf("payload size         : %zu bytes\n",
           sizeof(hasee_frame_packet_t));
    printf("warm-up              : %d\n", warmup);
    printf("requested samples    : %d\n", samples);
    printf("valid samples        : %d\n", valid);
    printf("lost/errors          : %d\n", lost);
    printf("mean RTT             : %.3f us\n", mean);
    printf("median RTT           : %.3f us\n",
           percentile(rtt_values, valid, 0.50));
    printf("p95 RTT              : %.3f us\n",
           percentile(rtt_values, valid, 0.95));
    printf("p99 RTT              : %.3f us\n",
           percentile(rtt_values, valid, 0.99));
    printf("max RTT              : %.3f us\n",
           valid > 0 ? rtt_values[valid - 1] : 0.0);
    printf("estimated one-way    : %.3f us "
           "(median RTT / 2)\n",
           percentile(rtt_values, valid, 0.50) / 2.0);
    printf("CSV                   : %s\n",
           csv_path);

    free(rtt_values);
    close(fd);

    return valid == samples ? 0 : 2;
}