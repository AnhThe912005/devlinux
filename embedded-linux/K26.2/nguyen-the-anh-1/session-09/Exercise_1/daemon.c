#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "sensor_shm.h"

int shmid;
sensor_data_t *data;

void cleanup(int sig)
{
    (void)sig;

    printf("\n[Daemon] Cleaning up shared memory. Goodbye.\n");

    if (shmdt(data) == -1) {
        perror("shmdt");
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

double read_cpu_temp(void)
{
    FILE *fp;
    double load1;

    fp = fopen("/proc/loadavg", "r");
    if (fp == NULL) {
        perror("fopen /proc/loadavg");
        return 40.0;
    }

    if (fscanf(fp, "%lf", &load1) != 1) {
        fprintf(stderr, "Failed to read CPU load\n");
        fclose(fp);
        return 40.0;
    }

    fclose(fp);

    return 40.0 + load1 * 10.0;
}

double read_ram_usage(void)
{
    FILE *fp;
    char line[256];

    unsigned long mem_total = 0;
    unsigned long mem_free = 0;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        perror("fopen /proc/meminfo");
        return 0.0;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (sscanf(line, "MemTotal: %lu kB", &mem_total) == 1) {
            continue;
        }

        if (sscanf(line, "MemFree: %lu kB", &mem_free) == 1) {
            continue;
        }
    }

    fclose(fp);

    if (mem_total == 0) {
        return 0.0;
    }

    return ((double)(mem_total - mem_free) / mem_total) * 100.0;
}

int main(void)
{
    signal(SIGINT, cleanup);

    shmid = shmget(
        SHM_KEY,
        sizeof(sensor_data_t),
        IPC_CREAT | 0666
    );

    if (shmid == -1) {
        perror("shmget");
        return EXIT_FAILURE;
    }

    data = (sensor_data_t *)shmat(shmid, NULL, 0);

    if (data == (void *)-1) {
        perror("shmat");

        if (shmctl(shmid, IPC_RMID, NULL) == -1) {
            perror("shmctl");
        }

        return EXIT_FAILURE;
    }

    printf("[Daemon] Shared memory created. Key=0x1234\n");

    while (1) {

        data->timestamp = time(NULL);

        data->cpu_temp = read_cpu_temp();

        data->ram_used_pct = read_ram_usage();

        printf(
            "[Daemon] Written: temp=%.2f ram=%.2f%%\n",
            data->cpu_temp,
            data->ram_used_pct
        );

        sleep(2);
    }

    return 0;
}
