CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TARGET = discop2
SOURCE = $(TARGET).c


all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o output/$(TARGET)

.PHONY: clean
clean:
	rm -f output/$(TARGET)

.PHONY: all
