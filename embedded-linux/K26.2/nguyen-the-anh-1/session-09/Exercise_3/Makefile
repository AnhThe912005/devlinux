CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=700
LDFLAGS = -lrt -lpthread

TARGETS = controller device

all: $(TARGETS)

controller: controller.c
	$(CC) $(CFLAGS) controller.c -o controller $(LDFLAGS)

device: device.c
	$(CC) $(CFLAGS) device.c -o device $(LDFLAGS)

clean:
	rm -f $(TARGETS)
