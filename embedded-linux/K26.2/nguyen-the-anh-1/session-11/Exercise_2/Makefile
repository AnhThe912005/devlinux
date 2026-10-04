CC = gcc

CFLAGS = -Wall -Wextra -Werror -std=c11

TARGETS = event_monitor event_client

all: $(TARGETS)

event_monitor: event_monitor.c event_monitor.h
	$(CC) $(CFLAGS) -o event_monitor event_monitor.c

event_client: event_client.c event_monitor.h
	$(CC) $(CFLAGS) -o event_client event_client.c

clean:
	rm -f $(TARGETS)
