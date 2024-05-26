# Makefile for CSE 344 HW4 Submission

# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -lrt -pthread

# Target Executable
TARGET = MWCp

# Source Files
SRCS = 200104004003_main.c

# Object Files
OBJS = $(SRCS:.c=.o)

# Default Rule
all: compile

# Compile Rule
compile: $(SRCS)
	$(CC) -o $(TARGET) $(SRCS) $(CFLAGS)

# Clean Rule
clean:
	@rm -f $(TARGET)
	@rm -f *.o
	@rm -f fifo1
	@rm -f fifo2

# Run Rule (Not used per requirements)
# run:
# 	@./$(TARGET) # This line should be commented out or removed per requirements

.PHONY: all compile clean

