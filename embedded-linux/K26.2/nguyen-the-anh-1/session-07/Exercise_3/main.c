#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

int main(void)
{
    sigset_t block_set;
    sigset_t old_set;

    /* Initialize signal set */
    sigemptyset(&block_set);
    sigaddset(&block_set, SIGINT);

    /* Repeat 5 transactions */
    for (int i = 1; i <= 5; i++)
    {
        /*
         * Block SIGINT and save the old signal mask
         */
        if (sigprocmask(SIG_BLOCK, &block_set, &old_set) == -1)
        {
            perror("sigprocmask block failed");
            exit(EXIT_FAILURE);
        }

        /* Critical section */
        printf("[SAFE] Writing transaction #%d ...\n", i);
        fflush(stdout);

        sleep(3);

        printf("[SAFE] Transaction #%d committed.\n", i);
        fflush(stdout);

        /*
         * Restore the previous signal mask
         */
        if (sigprocmask(SIG_SETMASK, &old_set, NULL) == -1)
        {
            perror("sigprocmask restore failed");
            exit(EXIT_FAILURE);
        }

        /* Idle section */
        printf("[IDLE] Waiting for next transaction...\n");
        fflush(stdout);

        sleep(3);
    }

    return 0;
}
