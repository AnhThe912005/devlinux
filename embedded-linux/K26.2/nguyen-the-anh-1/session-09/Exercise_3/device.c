#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <pthread.h>

#define SHM_NAME "/device_shm"

typedef struct {
    pthread_mutex_t mutex;
    int status;
} device_state_t;

volatile sig_atomic_t running = 1;

void handle_sigint(int sig)
{
    (void)sig;
    running = 0;
}

int main(void)
{
    int fd;
    device_state_t *state;

    signal(SIGINT, handle_sigint);

    /*
     * Open existing shared memory.
     * Do NOT use O_CREAT here.
     */
    fd = shm_open(SHM_NAME, O_RDWR, 0666);

    if (fd == -1) {
        perror("shm_open");
        fprintf(stderr, "Please start controller first.\n");
        return EXIT_FAILURE;
    }

    /*
     * Map shared memory
     */
    state = mmap(
        NULL,
        sizeof(device_state_t),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (state == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);

    printf("[Device] Attached to %s\n", SHM_NAME);

    while (running) {

        int current_status;

        /*
         * Lock shared mutex before reading
         */
        if (pthread_mutex_lock(&state->mutex) != 0) {
            fprintf(stderr, "pthread_mutex_lock failed\n");
            break;
        }

        current_status = state->status;

        if (pthread_mutex_unlock(&state->mutex) != 0) {
            fprintf(stderr, "pthread_mutex_unlock failed\n");
            break;
        }

        if (current_status == 1) {
            printf("[Device] Status: ON  — Running...\n");
        }
        else {
            printf("[Device] Status: OFF — Idle.\n");
        }

        sleep(1);
    }

    /*
     * Unmap shared memory
     */
    if (munmap(state, sizeof(device_state_t)) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }

    printf("[Device] Detached. Goodbye.\n");

    return EXIT_SUCCESS;
}
