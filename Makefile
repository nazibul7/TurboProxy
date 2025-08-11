# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

# Version
VERSION ?= v1-single-threaded

# Directories
VERSION_SRC_DIR = src/$(VERSION)
COMMON_SRC_DIR = src/common
OBJDIR = build/$(VERSION)

# Sources (version-specific + only the common files you need)
VERSION_SOURCES = $(wildcard $(VERSION_SRC_DIR)/*.c)
COMMON_SOURCES = \
    $(COMMON_SRC_DIR)/server.c \
    $(COMMON_SRC_DIR)/error_handler.c \
    $(COMMON_SRC_DIR)/route_config.c \
    $(COMMON_SRC_DIR)/request_parser.c \
    $(COMMON_SRC_DIR)/proxy.c \
    $(COMMON_SRC_DIR)/rebuild_request.c

SOURCES = $(VERSION_SOURCES) $(COMMON_SOURCES)
OBJECTS = $(patsubst %.c, $(OBJDIR)/%.o, $(notdir $(SOURCES)))

# Target
TARGET = $(VERSION)-server

# Default rule
all: $(TARGET)

# Link
$(TARGET): $(OBJECTS)
	@mkdir -p bin
	$(CC) $(OBJECTS) -o bin/$@

# Compile (pattern rule for both version-specific and common)
$(OBJDIR)/%.o: $(VERSION_SRC_DIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(COMMON_SRC_DIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf build bin *.o *-server

# Debug rule to see what files will be compiled
show-files:
	@echo "VERSION: $(VERSION)"
	@echo "VERSION_SOURCES: $(VERSION_SOURCES)"
	@echo "COMMON_SOURCES: $(COMMON_SOURCES)"
	@echo "ALL SOURCES: $(SOURCES)"
	@echo "OBJECTS: $(OBJECTS)"

.PHONY: all clean show-files