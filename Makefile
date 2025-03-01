# Makefile

# Compiler and Flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -I include

# Directories
SRC_DIR = src
OBJ_DIR = obj

# Source and Object Files
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC_FILES))

# Target Executable
TARGET = filesys

# Default Target
all: $(TARGET)

# Link Object Files to Create Executable
$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^

# Compile .c Files to .o Files in obj/
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean Generated Files
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

