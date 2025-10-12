CC = gcc
CFLAGS = -Wall -std=gnu99
TARGET = peer
SRCS = peer.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS)

clean:
	rm -f $(TARGET)
