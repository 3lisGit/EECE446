# Makefile for EECE 446 Program 2
# Fall 2025
# [Your Names Here]

CC = gcc
CFLAGS = -Wall -std=gnu99
TARGET = peer

all: $(TARGET)

$(TARGET): peer.c
	$(CC) $(CFLAGS) -o $(TARGET) peer.c

clean:
	rm -f $(TARGET)