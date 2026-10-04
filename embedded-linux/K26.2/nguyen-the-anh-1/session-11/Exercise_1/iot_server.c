#include "iot_server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_server = 0;

static void handle_sigint(int sig)
{
    (void)sig;
    stop_server = 1;
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

static void remove_client(client_t clients[], int index, fd_set *master)
{
    if (clients[index].fd >= 0) {
        FD_CLR(clients[index].fd, master);
        close(clients[index].fd);
        clients[index].fd = -1;
        clients[index].mode = 0;
        clients[index].last_activity = 0;
    }
}

static int count_clients(client_t clients[])
{
    int count = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0)
            count++;
    }

    return count;
}

static void broadcast_status(client_t clients[], int global_mode)
{
    char message[256];

    double temp = 25.0 + (rand() % 10);
    double humidity = 30.0 + (rand() % 30);
    int clients_count = count_clients(clients);

    snprintf(
        message,
        sizeof(message),
        "[BROADCAST] Temp=%.1f Humidity=%.1f Mode=%d Clients=%d\n",
        temp,
        humidity,
        global_mode,
        clients_count
    );

    printf(
        "[Server] Broadcasting to %d clients: "
        "Temp=%.1f Humidity=%.1f Mode=%d Clients=%d\n",
        clients_count,
        temp,
        humidity,
        global_mode,
        clients_count
    );

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0) {
            if (send_all(clients[i].fd, message, strlen(message)) < 0) {
                perror("[Server] send");
                clients[i].fd = -1;
            }
        }
    }
}

int main(void)
{
    int server_fd;
    int max_fd;
    int client_count;

    struct sockaddr_in server_addr;

    client_t clients[MAX_CLIENTS];

    fd_set master_fds;
    fd_set read_fds;

    struct timeval timeout;

    time_t last_broadcast;

    int global_mode = 0;

    srand((unsigned int)time(NULL));

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].mode = 0;
        clients[i].last_activity = 0;
    }

    signal(SIGINT, handle_sigint);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("[Server] socket");
        return EXIT_FAILURE;
    }

    int opt = 1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &opt, sizeof(opt)) < 0) {
        perror("[Server] setsockopt");
        close(server_fd);
        return EXIT_FAILURE;
    }

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("[Server] bind");
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("[Server] listen");
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[Server] Listening on localhost:%d\n", SERVER_PORT);

    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);

    max_fd = server_fd;

    last_broadcast = time(NULL);

    while (!stop_server) {
        read_fds = master_fds;

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int ready = select(max_fd + 1,
                           &read_fds,
                           NULL,
                           NULL,
                           &timeout);

        if (ready < 0) {
            if (errno == EINTR)
                continue;

            perror("[Server] select");
            break;
        }

        /* New client connection */
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);

            int client_fd = accept(
                server_fd,
                (struct sockaddr *)&client_addr,
                &client_len
            );

            if (client_fd < 0) {
                if (errno != EINTR)
                    perror("[Server] accept");
            } else {
                int slot = -1;

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd < 0) {
                        slot = i;
                        break;
                    }
                }

                if (slot < 0) {
                    const char *msg = "ERROR: Server full\n";
                    send_all(client_fd, msg, strlen(msg));
                    close(client_fd);
                } else {
                    clients[slot].fd = client_fd;
                    clients[slot].mode = 0;
                    clients[slot].last_activity = time(NULL);

                    FD_SET(client_fd, &master_fds);

                    if (client_fd > max_fd)
                        max_fd = client_fd;

                    char client_ip[INET_ADDRSTRLEN];

                    inet_ntop(AF_INET,
                              &client_addr.sin_addr,
                              client_ip,
                              sizeof(client_ip));

                    client_count = count_clients(clients);

                    printf(
                        "[Server] Client %d connected from %s:%d\n",
                        slot + 1,
                        client_ip,
                        ntohs(client_addr.sin_port)
                    );

                    printf("[Server] Active clients: %d\n",
                           client_count);
                }
            }
        }

        /* Handle existing clients */
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;

            if (fd < 0)
                continue;

            if (!FD_ISSET(fd, &read_fds))
                continue;

            char buffer[1024];

            ssize_t n = recv(fd, buffer, sizeof(buffer) - 1, 0);

            if (n < 0) {
                if (errno == EINTR)
                    continue;

                perror("[Server] recv");
                remove_client(clients, i, &master_fds);
                continue;
            }

            if (n == 0) {
                printf("[Server] Client %d disconnected\n", i + 1);
                remove_client(clients, i, &master_fds);
                continue;
            }

            buffer[n] = '\0';

            clients[i].last_activity = time(NULL);

            /* Remove newline */
            buffer[strcspn(buffer, "\r\n")] = '\0';

            printf("[Server] Client %d command: %s\n",
                   i + 1,
                   buffer);

            if (strcmp(buffer, "GET_TEMP") == 0) {
                double temp = 25.0 + (rand() % 10);

                char response[128];

                snprintf(response,
                         sizeof(response),
                         "%.1f\n",
                         temp);

                printf("[Server] Client %d: GET_TEMP -> %.1f\n",
                       i + 1,
                       temp);

                if (send_all(fd, response, strlen(response)) < 0) {
                    perror("[Server] send");
                    remove_client(clients, i, &master_fds);
                }
            }

            else if (strcmp(buffer, "GET_HUMIDITY") == 0) {
                double humidity = 30.0 + (rand() % 30);

                char response[128];

                snprintf(response,
                         sizeof(response),
                         "%.1f\n",
                         humidity);

                printf(
                    "[Server] Client %d: GET_HUMIDITY -> %.1f\n",
                    i + 1,
                    humidity
                );

                if (send_all(fd, response, strlen(response)) < 0) {
                    perror("[Server] send");
                    remove_client(clients, i, &master_fds);
                }
            }

            else if (strncmp(buffer, "SET_MODE ", 9) == 0) {
                int mode;

                if (sscanf(buffer + 9, "%d", &mode) == 1 &&
                    mode >= 0 && mode <= 2) {

                    global_mode = mode;
                    clients[i].mode = mode;

                    const char *response = "OK\n";

                    printf(
                        "[Server] Client %d: SET_MODE %d -> OK\n",
                        i + 1,
                        mode
                    );

                    if (send_all(fd,
                                 response,
                                 strlen(response)) < 0) {
                        perror("[Server] send");
                        remove_client(clients, i, &master_fds);
                    }
                } else {
                    const char *response = "ERROR: Invalid mode\n";

                    send_all(fd,
                             response,
                             strlen(response));
                }
            }

            else if (strcmp(buffer, "QUIT") == 0 ||
                     strcmp(buffer, "quit") == 0) {

                printf("[Server] Client %d requested QUIT\n",
                       i + 1);

                remove_client(clients, i, &master_fds);
            }

            else {
                const char *response =
                    "ERROR: Unknown command\n";

                if (send_all(fd,
                             response,
                             strlen(response)) < 0) {
                    perror("[Server] send");
                    remove_client(clients, i, &master_fds);
                }
            }
        }

        /* Broadcast every 5 seconds */
        time_t now = time(NULL);

        if (now - last_broadcast >= BROADCAST_INTERVAL) {
            broadcast_status(clients, global_mode);
            last_broadcast = now;
        }
    }

    /* Cleanup */
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd >= 0) {
            close(clients[i].fd);
            clients[i].fd = -1;
        }
    }

    close(server_fd);

    printf("[Server] Shutdown complete.\n");

    return EXIT_SUCCESS;
}
