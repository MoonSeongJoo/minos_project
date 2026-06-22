// #include <stdio.h>
// #include <stdint.h>
// #include <time.h>
// #include <unistd.h>
// #include <string.h>

// struct ai_meta {
//     uint32_t seq;
//     uint64_t timestamp_ns;
//     uint32_t obj_count;
//     uint32_t confidence;
//     uint32_t uncertainty;
//     uint32_t latency_ms;
//     uint32_t frame_drop;
//     uint32_t health;
//     uint32_t crc32;
// };

// static uint64_t now_ns(void) {
//     struct timespec ts;
//     clock_gettime(CLOCK_MONOTONIC, &ts);
//     return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
// }

// static uint32_t simple_crc32(const void *buf, size_t len) {
//     const uint8_t *p = (const uint8_t *)buf;
//     uint32_t crc = 0x12345678U;
//     for (size_t i = 0; i < len; i++) {
//         crc ^= p[i];
//         crc = (crc << 5) | (crc >> 27);
//         crc += 0x9e3779b9U;
//     }
//     return crc;
// }

// int main(void) {
//     struct ai_meta m;
//     memset(&m, 0, sizeof(m));

//     while (1) {
//         m.seq++;
//         m.timestamp_ns = now_ns();
//         m.obj_count = 3 + (m.seq % 5);
//         m.confidence = 90 - (m.seq % 7);

//         if ((m.seq % 20) >= 15) {
//             m.uncertainty = 85 + (m.seq % 10);
//             m.latency_ms = 40 + (m.seq % 10);
//             m.frame_drop = 1;
//             m.health = 1;
//         } else {
//             m.uncertainty = 8 + (m.seq % 6);
//             m.latency_ms = 15 + (m.seq % 5);
//             m.frame_drop = 0;
//             m.health = 0;
//         }

//         m.crc32 = 0;
//         m.crc32 = simple_crc32(&m, sizeof(m));

//         printf("[AI-STUB] seq=%u ts=%llu obj=%u conf=%u unc=%u lat=%u drop=%u health=%u crc=0x%08x\n",
//                m.seq,
//                (unsigned long long)m.timestamp_ns,
//                m.obj_count,
//                m.confidence,
//                m.uncertainty,
//                m.latency_ms,
//                m.frame_drop,
//                m.health,
//                m.crc32);
//         fflush(stdout);

//         sleep(1);
//     }

//     return 0;
// }

#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <errno.h>

struct ai_meta {
    uint32_t seq;
    uint64_t timestamp_ns;
    uint32_t obj_count;
    uint32_t confidence;
    uint32_t uncertainty;
    uint32_t latency_ms;
    uint32_t frame_drop;
    uint32_t health;
    uint32_t crc32;
};

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static uint32_t simple_crc32(const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t crc = 0x12345678U;
    for (size_t i = 0; i < len; i++) {
        crc ^= p[i];
        crc = (crc << 5) | (crc >> 27);
        crc += 0x9e3779b9U;
    }
    return crc;
}

int main(void) {
    struct ai_meta m;
    char msg[256];

    int sock;
    struct sockaddr_in dst;

    memset(&m, 0, sizeof(m));
    memset(&dst, 0, sizeof(dst));

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    dst.sin_family = AF_INET;
    dst.sin_port = htons(5000);
    dst.sin_addr.s_addr = inet_addr("10.10.10.1");

    while (1) {
        m.seq++;
        m.timestamp_ns = now_ns();
        m.obj_count = 3 + (m.seq % 5);
        m.confidence = 90 - (m.seq % 7);

        if ((m.seq % 20) >= 15) {
            m.uncertainty = 85 + (m.seq % 10);
            m.latency_ms = 40 + (m.seq % 10);
            m.frame_drop = 1;
            m.health = 1;
        } else {
            m.uncertainty = 8 + (m.seq % 6);
            m.latency_ms = 15 + (m.seq % 5);
            m.frame_drop = 0;
            m.health = 0;
        }

        m.crc32 = 0;
        m.crc32 = simple_crc32(&m, sizeof(m));

        snprintf(msg, sizeof(msg),
                 "[AI-STUB] seq=%u ts=%llu obj=%u conf=%u unc=%u lat=%u drop=%u health=%u crc=0x%08x\n",
                 m.seq,
                 (unsigned long long)m.timestamp_ns,
                 m.obj_count,
                 m.confidence,
                 m.uncertainty,
                 m.latency_ms,
                 m.frame_drop,
                 m.health,
                 m.crc32);

        /* guest 콘솔 출력 */
        printf("%s", msg);
        fflush(stdout);

        /* host(10.10.10.1:5000)로 UDP 전송 */
        if (sendto(sock, msg, strlen(msg), 0,
                   (struct sockaddr *)&dst, sizeof(dst)) < 0) {
            perror("sendto");
        }

        sleep(1);
    }

    return 0;
}