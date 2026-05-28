BNFC ?= bnfc
GRAMMAR := Grammar.cf
CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11

.PHONY: all gen build test clean distclean

all: build

gen:
	$(BNFC) --c $(GRAMMAR)

build: gen TestInput

TestInput: Absyn.o Buffer.o Lexer.o Parser.o Printer.o Test.o
	$(CC) $(CFLAGS) -o $@ Absyn.o Buffer.o Lexer.o Parser.o Printer.o Test.o

Absyn.o: Absyn.c Absyn.h
	$(CC) $(CFLAGS) -c Absyn.c

Buffer.o: Buffer.c Buffer.h
	$(CC) $(CFLAGS) -c Buffer.c

Lexer.c: Grammar.l
	flex -Pgrammar_ -oLexer.c Grammar.l

Parser.c Bison.h: Grammar.y
	perl -pi -e 's/%defines "Bison.h"//' Grammar.y
	perl -pi -e 's/extern int yylex/extern int grammar_lex/; s/extern int grammar_lex(.*)/extern int grammar_lex$$1\n#define yylex grammar_lex/' Grammar.y
	bison -t --defines=Bison.h Grammar.y -o Parser.c

Lexer.o: Lexer.c Bison.h
	$(CC) $(CFLAGS) -Wno-sign-conversion -c Lexer.c

Parser.o: Parser.c Absyn.h Bison.h
	$(CC) $(CFLAGS) -c Parser.c

Printer.o: Printer.c Printer.h Absyn.h
	$(CC) $(CFLAGS) -c Printer.c

Test.o: Test.c Parser.h Printer.h Absyn.h
	$(CC) $(CFLAGS) -c Test.c

test: build
	printf "echo hello | wc > out.txt\n" | ./TestInput

clean:
	rm -f *.o TestInput Parser.c Parser.h Lexer.c Bison.h Absyn.c Absyn.h
	rm -f Buffer.c Buffer.h Printer.c Printer.h Skeleton.c Skeleton.h Test.c
	rm -f Grammar.l Grammar.y Grammar.y.bak

distclean: clean
	rm -f Makefile.generated
