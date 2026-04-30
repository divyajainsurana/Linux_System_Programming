CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = myshell

SRC = main.c \
      cmd_localdate.c \
      cmd_ls.c \
      cmd_cat.c \
      cmd_pkg.c \
      registry.c \
      register_all_commands.c \
      $(wildcard argtable3/src/*.c)

OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: all
	./$(TARGET)

help: all
	./$(TARGET) localdate -h

test: all
	./$(TARGET) localdate
	./$(TARGET) localdate -h
	./$(TARGET) ls
	./$(TARGET) ls -a
	./$(TARGET) cat Makefile
	./$(TARGET) pkg
