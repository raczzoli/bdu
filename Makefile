# Compiler and flags
CC = gcc
CFLAGS = -std=gnu99 -Wall -Wextra -g
LDFLAGS = -lpthread

# Target executable
PROG = bdu

# Source files
SRCS = main.c dir.c queue.c output.c utils.c
OBJS = $(SRCS:.c=.o)

# Default target
all: $(PROG)

# Link object files into the final executable
$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(PROG)

install:
	mkdir -p $(DESTDIR)/usr/bin
	install -m 0755 $(PROG) $(DESTDIR)/usr/bin/bdu

install2:
	sudo install $(PROG) /usr/bin/


# Phony targets
.PHONY: all clean