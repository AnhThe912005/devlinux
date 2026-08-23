#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define LOG_ERR     "<3>"
#define LOG_WARNING "<4>"
#define LOG_INFO    "<6>"

int main(void) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);

    srand(time(NULL));

    int cycle = 0;
    int elapsed = 0;

    while (elapsed < 30) {
        cycle++;

        fprintf(stderr, LOG_INFO    "Service running normally, cycle %d\n", cycle);
        fprintf(stderr, LOG_WARNING "Memory usage high: %d%%\n", 80 + rand() % 15);
        fprintf(stderr, LOG_ERR     "Failed to connect to database, retry %d\n", cycle);

        sleep(2);
        elapsed += 2;
    }

    abort();
    return 0;
}
