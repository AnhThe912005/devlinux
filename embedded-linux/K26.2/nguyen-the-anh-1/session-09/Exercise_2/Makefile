CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_XOPEN_SOURCE=700

TARGETS = config-writer config-reader

all: $(TARGETS)

config-writer: writer.c device_cfg.h
	$(CC) $(CFLAGS) writer.c -o config-writer

config-reader: reader.c device_cfg.h
	$(CC) $(CFLAGS) reader.c -o config-reader

clean:
	rm -f $(TARGETS)
