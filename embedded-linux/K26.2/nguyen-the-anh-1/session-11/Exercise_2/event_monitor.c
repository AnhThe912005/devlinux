#include "event_monitor.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stop_monitor = 0;

static void handle_sigterm(int sig)
{
    (void)sig;
    stop_monitor = 1;
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

static int create_fifo(void)
{
    if (mkfifo(FIFO_PATH, 0666) < 0) {
        if (errno != EEXIST) {
            perror("[Monitor] mkfifo");
            return -1;
        }
    }

    return 0;
}

static int create_unix_socket(void)
{
    int fd;
    struct sockaddr_un addr;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("[Monitor] socket");
        return -1;
    }

    unlink(SOCKET_PATH);

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path,
            SOCKET_PATH,
            sizeof(addr.sun_path) - 1);

    if (bind(fd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0) {
        perror("[Monitor] bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 10) < 0) {
        perror("[Monitor] listen");
        close(fd);
        unlink(SOCKET_PATH);
        return -1;
    }

    return fd;
}

static int open_status_file(void)
{
    int fd;

    fd = open(FILE_PATH, O_RDONLY | O_CREAT, 0666);

    if (fd < 0) {
        perror("[Monitor] open system_status");
        return -1;
    }

    return fd;
}

static void handle_socket_command(
    int client_fd,
    char *command,
    int *active,
    int *events_seen,
    time_t start_time)
{
    command[strcspn(command, "\r\n")] = '\0';

    if (strcmp(command, "STATUS") == 0) {
        time_t now = time(NULL);
        long uptime = (long)(now - start_time);

        char response[256];

        snprintf(response,
                 sizeof(response),
                 "Total events: %d, Uptime: %ld seconds\n",
                 *events_seen,
                 uptime);

        if (send_all(client_fd,
                     response,
                     strlen(response)) < 0) {
            perror("[Monitor] send");
        }
    }

    else if (strcmp(command, "START") == 0) {
        *active = 1;

        const char *response = "OK\n";

        if (send_all(client_fd,
                     response,
                     strlen(response)) < 0) {
            perror("[Monitor] send");
        }

        printf("[Monitor] Monitoring STARTED\n");
    }

    else if (strcmp(command, "STOP") == 0) {
        *active = 0;

        const char *response = "OK\n";

        if (send_all(client_fd,
                     response,
                     strlen(response)) < 0) {
            perror("[Monitor] send");
        }

        printf("[Monitor] Monitoring STOPPED\n");
    }

    else if (strcmp(command, "EXIT") == 0) {
        const char *response = "OK\n";

        if (send_all(client_fd,
                     response,
                     strlen(response)) < 0) {
            perror("[Monitor] send");
        }

        printf("[Monitor] Client requested EXIT\n");
    }

    else {
        const char *response = "ERROR: Unknown command\n";

        if (send_all(client_fd,
                     response,
                     strlen(response)) < 0) {
            perror("[Monitor] send");
        }
    }
}

int main(void)
{
    int fifo_fd = -1;
    int socket_fd = -1;
    int file_fd = -1;

    struct pollfd poll_fds[NUM_POLL_FDS];

    struct stat file_stat;

    off_t last_file_size = 0;

    int events_seen = 0;
    int active = 1;

    time_t start_time;

    signal(SIGTERM, handle_sigterm);

    /*
     * Create FIFO
     */
    if (create_fifo() < 0)
        return EXIT_FAILURE;

    /*
     * Open FIFO in non-blocking mode.
     *
     * O_RDWR prevents read side from receiving EOF
     * whenever there is temporarily no writer.
     */
    fifo_fd = open(FIFO_PATH, O_RDWR | O_NONBLOCK);

    if (fifo_fd < 0) {
        perror("[Monitor] open FIFO");
        return EXIT_FAILURE;
    }

    /*
     * Create Unix domain socket
     */
    socket_fd = create_unix_socket();

    if (socket_fd < 0) {
        close(fifo_fd);
        return EXIT_FAILURE;
    }

    /*
     * Open system status file
     */
    file_fd = open_status_file();

    if (file_fd < 0) {
        close(fifo_fd);
        close(socket_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    /*
     * Get initial file size
     */
    if (stat(FILE_PATH, &file_stat) < 0) {
        perror("[Monitor] stat");
        close(fifo_fd);
        close(socket_fd);
        close(file_fd);
        unlink(SOCKET_PATH);
        return EXIT_FAILURE;
    }

    last_file_size = file_stat.st_size;

    /*
     * Initialize pollfd array
     */
    poll_fds[FD_FIFO].fd = fifo_fd;
    poll_fds[FD_FIFO].events = POLLIN;

    poll_fds[FD_SOCKET_LISTENER].fd = socket_fd;
    poll_fds[FD_SOCKET_LISTENER].events = POLLIN;

    /*
     * Regular files are always considered readable by poll().
     * We don't actually read from the file.
     * The file size is checked with stat().
     */
    poll_fds[FD_FILE].fd = file_fd;
    poll_fds[FD_FILE].events = POLLERR;

    start_time = time(NULL);

    printf("[Monitor] Listening on %s\n", SOCKET_PATH);

    printf("[Monitor] Monitoring %s (FIFO) and %s\n",
           FIFO_PATH,
           FILE_PATH);

    while (!stop_monitor) {

        int ret = poll(poll_fds,
                       NUM_POLL_FDS,
                       POLL_TIMEOUT_MS);

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            perror("[Monitor] poll");
            break;
        }

        /*
         * poll timeout
         */
        if (ret == 0) {
            printf("[HEARTBEAT] Monitor alive, events_seen=%d\n",
                   events_seen);
        }

        /*
         * FIFO event
         */
        if (poll_fds[FD_FIFO].revents & POLLIN) {

            char buffer[1024];

            ssize_t n;

            while ((n = read(fifo_fd,
                             buffer,
                             sizeof(buffer) - 1)) > 0) {

                buffer[n] = '\0';

                /*
                 * Process every line received.
                 */
                char *line = strtok(buffer, "\n");

                while (line != NULL) {

                    if (active) {
                        printf("[FIFO_EVENT] %s\n", line);
                        events_seen++;
                    }

                    line = strtok(NULL, "\n");
                }
            }

            if (n < 0 && errno != EAGAIN &&
                errno != EWOULDBLOCK) {
                perror("[Monitor] FIFO read");
            }
        }

        /*
         * Unix socket event
         */
        if (poll_fds[FD_SOCKET_LISTENER].revents & POLLIN) {

            int client_fd;

            client_fd = accept(socket_fd, NULL, NULL);

            if (client_fd < 0) {
                if (errno != EINTR)
                    perror("[Monitor] accept");
            } else {

                char command[256];

                ssize_t n = recv(client_fd,
                                 command,
                                 sizeof(command) - 1,
                                 0);

                if (n < 0) {
                    if (errno != EINTR)
                        perror("[Monitor] recv");
                }

                else if (n > 0) {
                    command[n] = '\0';

                    handle_socket_command(
                        client_fd,
                        command,
                        &active,
                        &events_seen,
                        start_time
                    );
                }

                close(client_fd);
            }
        }

        /*
         * Check file size.
         *
         * A regular file doesn't provide useful change
         * notifications through poll(), so stat() is used.
         */
        if (stat(FILE_PATH, &file_stat) < 0) {
            perror("[Monitor] stat");
        } else {

            if (file_stat.st_size != last_file_size) {

                if (active) {
                    printf(
                        "[FILE_EVENT] %s size changed to %ld bytes\n",
                        FILE_PATH,
                        (long)file_stat.st_size
                    );

                    events_seen++;
                }

                last_file_size = file_stat.st_size;
            }
        }
    }

    /*
     * Cleanup
     */
    if (fifo_fd >= 0)
        close(fifo_fd);

    if (socket_fd >= 0)
        close(socket_fd);

    if (file_fd >= 0)
        close(file_fd);

    unlink(SOCKET_PATH);

    printf("[Monitor] Shutdown complete.\n");

    return EXIT_SUCCESS;
}
