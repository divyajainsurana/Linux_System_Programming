CC = gcc
CFLAGS = -Wall -Wextra -std=c11

Unix_Shell: Unix_Shell.o token.o command.o builtin.o
	$(CC) $(CFLAGS) Unix_Shell.o token.o command.o builtin.o -o Unix_Shell

Unix_Shell.o: Unix_Shell.c token.h command.h builtin.h
	$(CC) $(CFLAGS) -c Unix_Shell.c

builtin.o: builtin.c builtin.h command.h
	$(CC) $(CFLAGS) -c builtin.c

command.o: command.c command.h
	$(CC) $(CFLAGS) -c command.c

token.o: token.c token.h
	$(CC) $(CFLAGS) -c token.c

clean:
	rm -f *.o Unix_Shell
