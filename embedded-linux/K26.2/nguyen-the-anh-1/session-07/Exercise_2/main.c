#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Signal handler for SIGUSR1 */
void handle_sigusr1(int sig)
{
    (void)sig;

    printf("[GATEWAY] Worker reported READY signal received\n");
    fflush(stdout);
}

int main(void)
{
    pid_t pid;
    int status;
    sigset_t block_set;

    /* Register SIGUSR1 handler */
    signal(SIGUSR1, handle_sigusr1);

    /* Create child process */
    pid = fork();

    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    /* ================= CHILD PROCESS ================= */
    else if (pid == 0)
    {
        printf("[WORKER] PID = %d\n", getpid());
        fflush(stdout);

        /* Simulate worker initialization */
        sleep(2);

        /* Send SIGUSR1 to parent */
        kill(getppid(), SIGUSR1);

        printf("[WORKER] Sent READY signal to gateway\n");
        fflush(stdout);

        /* Exit with code 7 */
        exit(7);
    }

    /* ================= PARENT PROCESS ================= */
    else
    {
        printf("[GATEWAY] Worker PID = %d\n", pid);
        fflush(stdout);

        /* Initialize signal set */
        sigemptyset(&block_set);
        sigaddset(&block_set, SIGUSR1);

        /* Block SIGUSR1 */
        if (sigprocmask(SIG_BLOCK, &block_set, NULL) == -1)
        {
            perror("sigprocmask block failed");
            exit(EXIT_FAILURE);
        }

        printf("[GATEWAY] SIGUSR1 blocked for 5 seconds...\n");
        fflush(stdout);

        /* Simulate gateway initialization */
        sleep(5);

        /* Unblock SIGUSR1 */
        if (sigprocmask(SIG_UNBLOCK, &block_set, NULL) == -1)
        {
            perror("sigprocmask unblock failed");
            exit(EXIT_FAILURE);
        }

        printf("[GATEWAY] SIGUSR1 unblocked\n");
        fflush(stdout);

        /* Wait for child process */
        wait(&status);

        if (WIFEXITED(status))
        {
            printf("[GATEWAY] Worker exited with code %d\n",
                   WEXITSTATUS(status));
        }
    }

    return 0;
}
