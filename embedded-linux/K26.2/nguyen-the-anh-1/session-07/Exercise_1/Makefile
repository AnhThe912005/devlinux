CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = sensor_daemon

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET)
