#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>

#include "sensor_shm.h"

int main(void)
{
    int shmid;
    sensor_data_t *data;

    shmid = shmget(
        SHM_KEY,
        sizeof(sensor_data_t),
        0666
    );

    if (shmid == -1) {
        fprintf(stderr, "Daemon is not running.\n");
        perror("shmget");
        return EXIT_FAILURE;
    }

    data = (sensor_data_t *)shmat(shmid, NULL, 0);

    if (data == (void *)-1) {
        perror("shmat");
        return EXIT_FAILURE;
    }

    printf("[Sensor Report]\n");
    printf("Timestamp : %ld\n", (long)data->timestamp);
    printf("CPU Temp  : %.2f C\n", data->cpu_temp);
    printf("RAM Used  : %.2f %%\n", data->ram_used_pct);

    if (shmdt(data) == -1) {
        perror("shmdt");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
