CC = gcc
CFLAGS = -Wall -std=gnu99
TARGET = peer

all: $(TARGET)

$(TARGET): peer.c
	$(CC) $(CFLAGS) -o $(TARGET) peer.c

clean:
	rm -f $(TARGET)