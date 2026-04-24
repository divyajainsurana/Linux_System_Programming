CC = gcc
CFLAGS = -Wall -Wextra -std=c11
TARGET = localdate

SRC = localdate_main.c cmd_localdate.c registry.c register_all_commands.c $(wildcard argtable3/src/*.c)
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
	./$(TARGET) -h
