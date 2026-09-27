#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "device_cfg.h"

#define CONFIG_FILE "/tmp/device.cfg"

void print_config(device_cfg_t *cfg)
{
    printf(
        "Current: baud_rate=%d sampling_rate=%d log_level=%d\n",
        cfg->baud_rate,
        cfg->sampling_rate_hz,
        cfg->log_level
    );
}

int main(void)
{
    int fd;
    device_cfg_t *cfg;
    char input[100];

    fd = open(CONFIG_FILE, O_RDWR | O_CREAT, 0666);

    if (fd == -1) {
        perror("open");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, sizeof(device_cfg_t)) == -1) {
        perror("ftruncate");
        close(fd);
        return EXIT_FAILURE;
    }

    cfg = mmap(
        NULL,
        sizeof(device_cfg_t),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (cfg == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return EXIT_FAILURE;
    }

    close(fd);

    /*
     * Initialize configuration if this is a new/empty config.
     */
    if (cfg->baud_rate != 9600 &&
        cfg->baud_rate != 115200 &&
        cfg->baud_rate != 460800) {

        cfg->baud_rate = 9600;
        cfg->sampling_rate_hz = 100;
        cfg->log_level = 2;

        if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) == -1) {
            perror("msync");
            munmap(cfg, sizeof(device_cfg_t));
            return EXIT_FAILURE;
        }
    }

    printf("[Config Writer] Loaded %s\n", CONFIG_FILE);

    print_config(cfg);

    while (1) {

        printf("\nSelect field to update [baud/rate/log/quit]: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "quit") == 0) {
            break;
        }

        /*
         * Change baud rate
         */
        if (strcmp(input, "baud") == 0) {

            printf("Select baud rate [9600/115200/460800]: ");

            if (fgets(input, sizeof(input), stdin) == NULL) {
                break;
            }

            input[strcspn(input, "\n")] = '\0';

            int baud = atoi(input);

            if (baud != 9600 &&
                baud != 115200 &&
                baud != 460800) {

                printf("Invalid baud rate.\n");
                continue;
            }

            cfg->baud_rate = baud;

            if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) == -1) {
                perror("msync");
                continue;
            }

            printf("[Updated] baud_rate = %d\n", cfg->baud_rate);
        }

        /*
         * Change sampling rate
         */
        else if (strcmp(input, "rate") == 0) {

            printf("Enter sampling rate [1-1000]: ");

            if (fgets(input, sizeof(input), stdin) == NULL) {
                break;
            }

            input[strcspn(input, "\n")] = '\0';

            int rate = atoi(input);

            if (rate < 1 || rate > 1000) {
                printf("Invalid sampling rate.\n");
                continue;
            }

            cfg->sampling_rate_hz = rate;

            if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) == -1) {
                perror("msync");
                continue;
            }

            printf(
                "[Updated] sampling_rate_hz = %d\n",
                cfg->sampling_rate_hz
            );
        }

        /*
         * Change log level
         */
        else if (strcmp(input, "log") == 0) {

            printf("Select log level [0=OFF, 1=ERROR, 2=INFO, 3=DEBUG]: ");

            if (fgets(input, sizeof(input), stdin) == NULL) {
                break;
            }

            input[strcspn(input, "\n")] = '\0';

            int level = atoi(input);

            if (level < 0 || level > 3) {
                printf("Invalid log level.\n");
                continue;
            }

            cfg->log_level = level;

            if (msync(cfg, sizeof(device_cfg_t), MS_SYNC) == -1) {
                perror("msync");
                continue;
            }

            printf(
                "[Updated] log_level = %d\n",
                cfg->log_level
            );
        }

        else {
            printf("Invalid option.\n");
        }
    }

    if (munmap(cfg, sizeof(device_cfg_t)) == -1) {
        perror("munmap");
        return EXIT_FAILURE;
    }

    printf("[Config Writer] Exiting.\n");

    return EXIT_SUCCESS;
}
