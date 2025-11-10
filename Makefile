CC = gcc
CFLAGS = -Wall -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
TARGET = peer


SOURCES = peer.c
OBJECTS = $(SOURCES:.c=.o)


all: $(TARGET)


$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(TARGET) $(OBJECTS)


.PHONY: all clean
