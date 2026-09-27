#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>

#define SHM_NAME "/device_shm"

typedef struct {
    pthread_mutex_t mutex;
    int status;
} device_state_t;

int main(void)
{
    int fd;
    device_state_t *state;
    pthread_mutexattr_t attr;
    char input[100];

    /*
     * Create/open shared memory
     */
    fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);

    if (fd == -1) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    /*
     * Set shared memory size
     */
    if (ftruncate(fd, sizeof(device_state_t)) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(SHM_NAME);
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
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    close(fd);

    /*
     * Initialize mutex attribute
     */
    if (pthread_mutexattr_init(&attr) != 0) {
        fprintf(stderr, "pthread_mutexattr_init failed\n");
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    /*
     * Make mutex usable between processes
     */
    if (pthread_mutexattr_setpshared(
            &attr,
            PTHREAD_PROCESS_SHARED) != 0) {

        fprintf(stderr, "pthread_mutexattr_setpshared failed\n");

        pthread_mutexattr_destroy(&attr);
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);

        return EXIT_FAILURE;
    }

    /*
     * Initialize mutex inside shared memory
     */
    if (pthread_mutex_init(&state->mutex, &attr) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");

        pthread_mutexattr_destroy(&attr);
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);

        return EXIT_FAILURE;
    }

    if (pthread_mutexattr_destroy(&attr) != 0) {
        fprintf(stderr, "pthread_mutexattr_destroy failed\n");

        pthread_mutex_destroy(&state->mutex);
        munmap(state, sizeof(device_state_t));
        shm_unlink(SHM_NAME);

        return EXIT_FAILURE;
    }

    /*
     * Initial device state
     */
    state->status = 0;

    printf(
        "[Controller] Shared memory ready. "
        "Commands: on / off / quit\n"
    );

    while (1) {

        printf("> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        /*
         * ON command
         */
        if (strcmp(input, "on") == 0) {

            if (pthread_mutex_lock(&state->mutex) != 0) {
                fprintf(stderr, "pthread_mutex_lock failed\n");
                break;
            }

            state->status = 1;

            if (pthread_mutex_unlock(&state->mutex) != 0) {
                fprintf(stderr, "pthread_mutex_unlock failed\n");
                break;
            }

            printf("[Controller] Command sent: ON\n");
        }

        /*
         * OFF command
         */
        else if (strcmp(input, "off") == 0) {

            if (pthread_mutex_lock(&state->mutex) != 0) {
                fprintf(stderr, "pthread_mutex_lock failed\n");
                break;
            }

            state->status = 0;

            if (pthread_mutex_unlock(&state->mutex) != 0) {
                fprintf(stderr, "pthread_mutex_unlock failed\n");
                break;
            }

            printf("[Controller] Command sent: OFF\n");
        }

        /*
         * QUIT command
         */
        else if (strcmp(input, "quit") == 0) {
            break;
        }

        else {
            printf("Unknown command. Use: on / off / quit\n");
        }
    }

    printf("[Controller] Cleaning up. Goodbye.\n");

    /*
     * Destroy mutex
     */
    if (pthread_mutex_destroy(&state->mutex) != 0) {
        fprintf(stderr, "pthread_mutex_destroy failed\n");
    }

    /*
     * Unmap shared memory
     */
    if (munmap(state, sizeof(device_state_t)) == -1) {
        perror("munmap");
    }

    /*
     * Remove shared memory object
     */
    if (shm_unlink(SHM_NAME) == -1) {
        perror("shm_unlink");
    }

    return EXIT_SUCCESS;
}
