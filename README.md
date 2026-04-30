# Week 03: Simple UNIX Shell

This branch contains the Week 03 shell assignment. The goal is to build a small UNIX-style shell in C that reads commands from the user, parses them, and executes built-in commands or external programs.

## Project Folder

```text
shell_implementation/
```

## Features

* Interactive prompt, starting with `%`
* Built-in `prompt` command to change the prompt text
* Built-in `pwd` command to print the current working directory
* Built-in `cd` command to change directories
* Built-in `exit` command to quit the shell
* External command execution with `fork()` and `execvp()`
* Sequential command execution using `;`
* Background execution using `&`
* Pipelines using `|`
* Input redirection using `<`
* Output redirection using `>`
* Wildcard expansion using `glob()`
* Signal handling so the shell ignores `Ctrl-C`, `Ctrl-\`, and `Ctrl-Z`
* Background child cleanup using `waitpid(..., WNOHANG)`

## Build

```sh
cd shell_implementation
make
```

## Run

```sh
./Unix_Shell
```

Example session:

```text
% pwd
% ls -l
% prompt shell$
shell$ echo hello
shell$ exit
```

## Test

```sh
cd shell_implementation
./test.sh
```

The test script builds the shell and checks built-ins, external commands, output redirection, and a simple pipeline.

## Source Files

```text
Unix_Shell.c   main shell loop, command execution, pipes, redirection
builtin.c      built-in shell commands
builtin.h      built-in command declarations
command.c      command separation, redirection parsing, wildcard expansion
command.h      command structure and separator definitions
token.c        input tokenization
token.h        token constants and tokenizer declaration
Makefile       build rules
test.sh        smoke tests
```
