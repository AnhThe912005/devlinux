CC = gcc

CFLAGS = -Wall -Wextra -Werror -std=c11

TARGETS = iot_server iot_client

all: $(TARGETS)

iot_server: iot_server.c iot_server.h
	$(CC) $(CFLAGS) -o iot_server iot_server.c

iot_client: iot_client.c iot_server.h
	$(CC) $(CFLAGS) -o iot_client iot_client.c

clean:
	rm -f $(TARGETS)
