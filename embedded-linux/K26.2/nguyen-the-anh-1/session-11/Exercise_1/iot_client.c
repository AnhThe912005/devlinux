#include "iot_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t stop_client = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    stop_client = 1;
}

static int send_all(int fd, const char *data, size_t len)
{
    size_t total = 0;

    while (total < len) {
        ssize_t n = send(fd, data + total, len - total, 0);

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

    struct sockaddr_in server_addr;

    signal(SIGINT, handle_sigint);

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (sock_fd < 0) {
        perror("[Client] socket");
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET,
                  "127.0.0.1",
                  &server_addr.sin_addr) <= 0) {
        perror("[Client] inet_pton");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    if (connect(sock_fd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("[Client] connect");
        close(sock_fd);
        return EXIT_FAILURE;
    }

    printf("[Client] Connected to localhost:%d\n", SERVER_PORT);
    printf("Commands: GET_TEMP, GET_HUMIDITY, SET_MODE <0|1|2>, QUIT\n");

    while (!stop_client) {
        char command[1024];

        printf("> ");
        fflush(stdout);

        if (fgets(command, sizeof(command), stdin) == NULL) {
            break;
        }

        command[strcspn(command, "\r\n")] = '\0';

        if (command[0] == '\0')
            continue;

        if (send_all(sock_fd,
                     command,
                     strlen(command)) < 0) {
            perror("[Client] send");
            break;
        }

        /*
         * Send newline so the server receives a complete command.
         */
        if (send_all(sock_fd, "\n", 1) < 0) {
            perror("[Client] send");
            break;
        }

        if (strcmp(command, "quit") == 0 ||
            strcmp(command, "QUIT") == 0) {
            break;
        }

        /*
         * Read server response.
         *
         * A response is newline terminated.
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
                stop_client = 1;
                break;
            }

            if (n == 0) {
                printf("[Client] Server disconnected.\n");
                stop_client = 1;
                break;
            }

            total += (size_t)n;
            response[total] = '\0';

            if (strchr(response, '\n') != NULL)
                break;
        }

        if (total > 0)
            printf("%s", response);
    }

    close(sock_fd);

    printf("[Client] Connection closed.\n");

    return EXIT_SUCCESS;
}
