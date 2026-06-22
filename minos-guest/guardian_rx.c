#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define GUARDIAN_PORT 7777

int main(void)
{
    int fd;
    int opt = 1;
    struct sockaddr_in addr;
    struct sockaddr_in src;
    char buf[512];
    char src_ip[INET_ADDRSTRLEN];

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("[VM1 Guardian] socket");
        return 1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(GUARDIAN_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[VM1 Guardian] bind");
        close(fd);
        return 1;
    }

    printf("[VM1 Guardian] listening on UDP %d\n", GUARDIAN_PORT);
    fflush(stdout);

    while (1) {
        socklen_t slen = sizeof(src);
        int n = recvfrom(fd, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&src, &slen);

        if (n < 0) {
            perror("[VM1 Guardian] recvfrom");
            continue;
        }

        buf[n] = '\0';

        inet_ntop(AF_INET, &src.sin_addr, src_ip, sizeof(src_ip));

        printf("[VM1 Guardian] RX from %s: %s\n", src_ip, buf);

        if (strstr(buf, "health=OOD") ||
            strstr(buf, "uncertainty=0.91") ||
            strstr(buf, "health=FAULT")) {
            printf("[VM1 Guardian] decision=PROBATION\n");
        } else {
            printf("[VM1 Guardian] decision=NORMAL\n");
        }

        fflush(stdout);
    }

    close(fd);
    return 0;
}