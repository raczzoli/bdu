# Compiler and flags
CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -g
LDFLAGS = -lpthread

# Target executable
TARGET = bdu

# Source files
SRCS = main.c queue.c output.c
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Link object files into the final executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

install:
	sudo cp -f ./bdu /usr/bin/bdu

# Phony targets
.PHONY: all clean