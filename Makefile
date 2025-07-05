# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

# Source and object files
SRCDIR = src
OBJDIR = build

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SOURCES))

# Output binary
TARGET = server_app

# Default rule
all: $(TARGET)

# Link object files into executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

# Compile .c to .o
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
