CC = gcc
CFLAGS = -Wall -std=c99 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
TARGET = peer

# Source files
SOURCES = peer.c
OBJECTS = $(SOURCES:.c=.o)

# Default target - builds the peer executable
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target - removes generated files
clean:
	rm -f $(TARGET) $(OBJECTS)

# Phony targets (not actual files)
.PHONY: all clean
