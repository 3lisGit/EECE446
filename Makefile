# Compiler and flags
CC = gcc
CFLAGS = -Wall -std=c99 -g
LDFLAGS = 

# Target executable name (must be named 'peer')
TARGET = peer

# Source files
SOURCES = peer.c
OBJECTS = $(SOURCES:.c=.o)

# Default target - builds the peer executable
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

# Compile source files to object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target - removes all generated files
clean:
	rm -f $(TARGET) $(OBJECTS)

# Phony targets (not actual files)
.PHONY: all clean

# Optional: help target
help:
	@echo "EECE 446 Program 2 Makefile"
	@echo "Available targets:"
	@echo "  all    - Build the peer executable (default)"
	@echo "  clean  - Remove all generated files"
	@echo "  help   - Show this help message"