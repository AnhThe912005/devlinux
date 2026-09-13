CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = transaction_logger

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

clean:
	rm -f $(TARGET)
