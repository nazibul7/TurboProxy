# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

# Versioned source directory (set this when calling make: make VERSION=v1-blocking)
VERSION ?= v1-blocking

SRCDIR = src/$(VERSION)
OBJDIR = build/$(VERSION)

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

# Output binary per version
TARGET = $(VERSION)-server

# Default rule
all: $(TARGET)

# Link object files into executable
$(TARGET): $(OBJECTS)
	@mkdir -p bin
	$(CC) $(OBJECTS) -o bin/$@

# Compile .c to .o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -rf build bin *.o *-server

.PHONY: all clean
