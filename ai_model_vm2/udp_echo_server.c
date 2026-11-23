#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define DEFAULT_PORT 7788
#define MAX_PACKET_SIZE 4096

int main(int argc, char **argv)
{
    int port = DEFAULT_PORT;

    if (argc >= 2)
        port = atoi(argv[1]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "invalid port: %d\n", port);
        return 1;
    }

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in local_addr;
    memset(&local_addr, 0, sizeof(local_addr));

    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons((uint16_t)port);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd,
             (struct sockaddr *)&local_addr,
             sizeof(local_addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    printf("[UDP Echo] listening on port %d\n", port);
    fflush(stdout);

    unsigned char buffer[MAX_PACKET_SIZE];

    while (1) {
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);

        ssize_t received = recvfrom(
            fd,
            buffer,
            sizeof(buffer),
            0,
            (struct sockaddr *)&peer_addr,
            &peer_len);

        if (received < 0) {
            if (errno == EINTR)
                continue;

            perror("recvfrom");
            continue;
        }

        ssize_t sent = sendto(
            fd,
            buffer,
            (size_t)received,
            0,
            (struct sockaddr *)&peer_addr,
            peer_len);

        if (sent != received)
            perror("sendto");
    }

    close(fd);
    return 0;
}