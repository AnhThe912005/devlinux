#include "event_monitor.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int send_all(int fd, const char *data, size_t len)
{
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd,
                         data + total,
                         len - total,
                         0);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        total += (size_t)n;
    }

    return 0;
}

int main(void)
{
    int sock_fd;

    struct sockaddr_un server_addr;

    sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (sock_fd < 0) {
        perror("[Client] socket");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sun_family = AF_UNIX;

    strncpy(server_addr.sun_path,
            SOCKET_PATH,
            sizeof(server_addr.sun_path) - 1);

    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("[Client] connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("[Client] Connected to %s\n", SOCKET_PATH);

    while (1) {

        char command[256];

        printf("> ");
        fflush(stdout);

        if (fgets(command,
                  sizeof(command),
                  stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\r\n")] = '\0';

        if (command[0] == '\0')
            continue;

        /*
         * Send command with newline.
         */
        if (send_all(sock_fd,
                     command,
                     strlen(command)) < 0) {
            perror("[Client] send");
            break;
        }

        if (send_all(sock_fd, "\n", 1) < 0) {
            perror("[Client] send");
            break;
        }

        /*
         * Receive response.
         */
        char response[1024];

        size_t total = 0;

        while (total < sizeof(response) - 1) {

            ssize_t n = recv(sock_fd,
                             response + total,
                             sizeof(response) - 1 - total,
                             0);

            if (n < 0) {
                if (errno == EINTR)
                    continue;

                perror("[Client] recv");
                close(sock_fd);
                return EXIT_FAILURE;
            }

            if (n == 0) {
                printf("[Client] Server closed connection.\n");
                close(sock_fd);
                return EXIT_SUCCESS;
            }

            total += (size_t)n;

            response[total] = '\0';

            if (strchr(response, '\n') != NULL)
                break;
        }

        printf("%s", response);

        /*
         * User can type exit/EXIT to leave client.
         */
        if (strcmp(command, "exit") == 0 ||
            strcmp(command, "EXIT") == 0) {
            break;
        }
    }

    close(sock_fd);

    printf("[Client] Connection closed.\n");

    return EXIT_SUCCESS;
}
