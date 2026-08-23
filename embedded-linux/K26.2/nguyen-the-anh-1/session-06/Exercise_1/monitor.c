#include <stdio.h>
#include <signal.h>
#include <unistd.h>

volatile sig_atomic_t stop_requested = 0;

void handle_sigterm(int sig) {
    (void)sig;
    stop_requested = 1;
}

int main(void) {
    setbuf(stdout, NULL);

    struct sigaction sa;
    sa.sa_handler = handle_sigterm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);

    int counter = 0;
    pid_t pid = getpid();

    while (!stop_requested) {
        counter++;
        printf("[monitor] Tick %d - PID %d\n", counter, pid);
        sleep(1);
    }

    printf("Service shutting down...\n");
    return 0;
}
