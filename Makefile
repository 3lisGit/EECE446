# EECE 446 Program 4 - P2P Registry Makefile
# Authors: Alexander Liu, Elijah Coleman

CC = gcc
CFLAGS = -Wall -g
DEBUG_FLAGS = -DDEBUG
TARGET = registry
SRCS = registry.c
OBJS = $(SRCS:.c=.o)

# Default target: build the registry
all: $(TARGET)

# Build the registry executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build with extra output
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

# Clean up generated files
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean debug
