CC := gcc
CFLAGS := -Wall -pthread

TARGET := booking

all: $(TARGET)

$(TARGET): booking.c
	$(CC) $(CFLAGS) booking.c -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
