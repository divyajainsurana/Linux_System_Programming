# BusyBox Shell Parser Migration

This document explains the difference between the previous command parsing
approach and the current BNFC grammar-based approach.

## Summary

The shell still uses the same command implementations:

- `cmd_echo.c`
- `cmd_ls.c`
- `cmd_cat.c`
- `cmd_wc.c`
- and the other `cmd_*.c` modules

The change is in how command lines are parsed before execution.

Previous flow:

```text
text line -> strtok/split_line -> argc/argv -> dispatch_command
```

Current flow:

```text
text line -> BNFC parser -> AST -> argc/argv or pipeline -> dispatch_command/execute_pipeline
```

The command registry is still the execution backend. BNFC is now the parsing
frontend.

## Presentation Overview

![Previous parser flow](images/old_parser_flow.svg)

![Current BNFC parser flow](images/bnfc_parser_flow.svg)

![Shell architecture layers](images/shell_layers.svg)

## Previous Approach: Manual Token Splitting

Earlier, simple commands were parsed by `split_line()` in `main.c`.

Example input:

```sh
echo hello world
```

The old parser split on whitespace:

```text
argv[0] = "echo"
argv[1] = "hello"
argv[2] = "world"
```

Then it called:

```c
dispatch_command(argc, argv);
```

Core snippet from the old approach:

```c
static int split_line(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    argv[argc] = NULL;
    return argc;
}
```

Pipelines were handled separately. A command like:

```sh
echo hello | wc
```

was split once by the pipe character:

```text
segment 1: echo hello
segment 2: wc
```

Then each segment was split again by whitespace.

This worked for basic cases, but the shell did not have a formal command-line
syntax. Each new shell feature required more manual string handling.

## Current Approach: BNFC Grammar

The grammar lives in:

```text
busybox_shell/bnfc/Grammar.cf
```

The key grammar rules are:

```bnf
StartInput. Input ::= [Job] ;

ForegroundJob. Job ::= CommandLine ;
BackgroundJob. Job ::= CommandLine "&" ;

separator nonempty Job ";" ;

CommandWithRedir. CommandLine ::= Pipeline [Redirection] ;

SingleCommand. Pipeline ::= CommandPart ;
PipeCommand. Pipeline ::= CommandPart "|" Pipeline ;

MkCommandPart. CommandPart ::= Word [Word] ;

InputRedirection. Redirection ::= "<" Word ;
OutputRedirection. Redirection ::= ">" Word ;
```

BNFC generates parser files from that grammar:

```text
Absyn.c / Absyn.h
Lexer.c
Parser.c / Parser.h
Printer.c / Printer.h
Test.c
```

Those files are generated during the build and are ignored by git.

The grammar parses input into an AST. The adapter code in `main.c` walks the AST
and converts it into the data structures the existing shell already knows how to
execute.

Important adapter functions:

```c
dispatch_bnfc_line()
bnfc_dispatch_job()
bnfc_dispatch_command_line()
bnfc_collect_pipeline()
bnfc_build_argv()
```

The entry point is:

```c
static int dispatch_bnfc_line(const char *line)
{
    Input input;
    ListJob jobs;
    int status = 0;

    input = psInput(line);
    if (input == NULL || input->kind != is_StartInput) {
        return 2;
    }

    jobs = input->u.startInput_.listjob_;
    while (jobs != NULL) {
        status = bnfc_dispatch_job(jobs->job_);
        if (status < 0) {
            break;
        }
        jobs = jobs->listjob_;
    }

    free_Input(input);
    return status;
}
```

The important idea is:

```text
BNFC does not execute commands.
It only gives main.c a structured AST to execute.
```

## Example: Simple Command

Input:

```sh
echo hello
```

BNFC parses it roughly as:

```text
StartInput [
  ForegroundJob (
    CommandWithRedir (
      SingleCommand (
        MkCommandPart "echo" ["hello"]
      )
      []
    )
  )
]
```

The adapter converts that AST into:

```text
argc = 2
argv = ["echo", "hello", NULL]
```

Then it calls:

```c
dispatch_command(argc, argv);
```

Adapter snippet:

```c
static int bnfc_build_argv(CommandPart part, char **argv, int max_args)
{
    ListWord words;
    int argc = 0;

    argv[argc++] = part->u.mkCommandPart_.word_;
    words = part->u.mkCommandPart_.listword_;

    while (words != NULL && argc < max_args - 1) {
        argv[argc++] = words->word_;
        words = words->listword_;
    }

    argv[argc] = NULL;
    return argc;
}
```

## Example: Pipeline

Input:

```sh
echo hello | wc
```

BNFC parses it roughly as:

```text
StartInput [
  ForegroundJob (
    CommandWithRedir (
      PipeCommand
        (MkCommandPart "echo" ["hello"])
        (SingleCommand (MkCommandPart "wc" []))
      []
    )
  )
]
```

The adapter converts it into:

```text
command_count = 2

command 1:
argc = 2
argv = ["echo", "hello", NULL]

command 2:
argc = 1
argv = ["wc", NULL]
```

Then it calls:

```c
execute_pipeline(command_count, argc_list, argv_list);
```

Adapter snippet:

```c
if (command_count == 1) {
    return bnfc_dispatch_simple_with_redirection(argc_list[0], argv_list[0],
                                                input_file, output_file);
}

return execute_pipeline(command_count, argc_list, argv_list);
```

## Example: Semicolon-Separated Jobs

Input:

```sh
pwd ; echo done
```

BNFC parses this as a list of jobs:

```text
StartInput [
  ForegroundJob (... pwd ...),
  ForegroundJob (... echo done ...)
]
```

The adapter executes each job in order.

## Example: Redirection

Input:

```sh
echo hello > out.txt
```

BNFC parses the output redirection as part of the command line. The adapter
opens `out.txt`, redirects `STDOUT_FILENO` with `dup2`, runs the existing
command implementation, and then restores stdout.

Pipeline redirection also works at the command-line level:

```sh
echo hello | wc > out.txt
```

Redirection snippet:

```c
static int bnfc_redirect_fd(const char *path, int target_fd, int flags, mode_t mode)
{
    int fd = open(path, flags, mode);

    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 1;
    }

    if (dup2(fd, target_fd) < 0) {
        fprintf(stderr, "dup2: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
```

For input redirection with pipelines, use the current grammar form:

```sh
cat | head -n 1 < Makefile
```

## What Changed In The Code

Added:

```text
busybox_shell/bnfc/Grammar.cf
busybox_shell/bnfc/Makefile
busybox_shell/bnfc/README.md
busybox_shell/bnfc/.gitignore
```

Updated:

```text
busybox_shell/Makefile
busybox_shell/main.c
```

The top-level Makefile now builds the BNFC parser before linking
`busybox_shell`.

Build snippet:

```make
BNFC_SRC = $(BNFC_DIR)/Absyn.c \
           $(BNFC_DIR)/Buffer.c \
           $(BNFC_DIR)/Lexer.c \
           $(BNFC_DIR)/Parser.c

bnfc_parser:
	$(MAKE) -C $(BNFC_DIR) build

$(TARGET): bnfc_parser $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
```

## What Did Not Change

The command modules did not need to change:

```text
cmd_echo.c
cmd_ls.c
cmd_cat.c
cmd_wc.c
...
```

They still receive normal `argc` and `argv` values.

The command registry also remains the backend:

```c
find_command(argv[0]);
cmd->run(argc, argv);
```

## Current Limitations

The current grammar is intentionally small. It supports:

```text
simple commands
pipelines
semicolon-separated jobs
top-level input/output redirection
```

It does not yet fully support:

```text
quoted strings: echo "hello world"
variables: echo $HOME
background execution: sleep 5 &
per-command redirection inside pipelines: cat < Makefile | head -n 1
append redirection: >>
```

Background jobs are parsed, but execution is not implemented yet.

## How To Verify

Build and run the BNFC command coverage test:

```sh
cd busybox_shell
source ~/.ghcup/env
export PATH="$HOME/.local/bin:$PATH"
make test-bnfc
```

Run the broader shell regression test:

```sh
make test
```

Inspect parser output directly:

```sh
cd busybox_shell/bnfc
make test
printf "echo hello | wc > out.txt\n" | ./TestInput
```

## Why This Is Better

The previous approach was easy to start with, but every shell feature required
more custom string manipulation.

The BNFC approach gives the shell a formal syntax:

```text
grammar rule -> AST node -> execution adapter
```

That makes future features easier to add systematically. Instead of adding
another string split, we add a grammar rule and then handle the corresponding
AST constructor.

## Slide-Friendly Talking Points

- The command modules did not change.
- The parser changed from manual string splitting to a formal grammar.
- BNFC gives us an AST, which makes pipelines and redirection explicit.
- `main.c` is now the bridge between syntax and execution.
- Future shell features should be added as grammar rules plus AST adapter cases.
