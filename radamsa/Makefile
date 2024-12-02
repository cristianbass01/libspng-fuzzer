# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 
LDFLAGS = -L./../libspng/build -lspng -Wl,-rpath=./../libspng/build

# Source files
SRCS = fuzz_main.c fuzz_spng_read.c fuzz_spng_write.c

# Object files (replace .c with .o in SRCS)
OBJS = $(SRCS:.c=.o)

# Target executable
TARGET = fuzz_main

# Default rule
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets
.PHONY: all clean
