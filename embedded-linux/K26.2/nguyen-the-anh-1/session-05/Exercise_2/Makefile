CC := gcc
CFLAGS := -Wall -pthread

TARGET := print_queue

all: $(TARGET)

$(TARGET): print_queue.c
	$(CC) $(CFLAGS) print_queue.c -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
