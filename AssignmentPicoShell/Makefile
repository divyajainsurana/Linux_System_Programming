CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_POSIX_C_SOURCE=200809L

TARGET = psh
HELPERS = myspin mysplit mystop myint

all: $(TARGET) $(HELPERS)

$(TARGET): psh.c
	$(CC) $(CFLAGS) psh.c -o $(TARGET)

myspin: myspin.c
	$(CC) $(CFLAGS) myspin.c -o myspin

mysplit: mysplit.c
	$(CC) $(CFLAGS) mysplit.c -o mysplit

mystop: mystop.c
	$(CC) $(CFLAGS) mystop.c -o mystop

myint: myint.c
	$(CC) $(CFLAGS) myint.c -o myint

test: all
	./test.sh

clean:
	rm -f $(TARGET) $(HELPERS) *.o
