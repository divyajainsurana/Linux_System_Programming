# BusyBox Shell BNFC Parser

This directory is the grammar-first frontend for `busybox_shell`.

The goal is to replace ad hoc parsing in `main.c` over time:

```text
line -> strtok/split_line -> dispatch_command
```

with:

```text
line -> BNFC parser -> AST -> existing dispatch_command / execute_pipeline
```

Step 1 defined the syntax and lets us inspect generated ASTs. The top-level
`busybox_shell` build now also links the generated parser and uses it for
interactive command lines before falling back to the old simple splitter.

## Build

```sh
source ~/.ghcup/env
export PATH="$HOME/.local/bin:$PATH"
make
```

## Try

```sh
printf "echo hello | wc > out.txt\n" | ./TestInput
```

## Next Integration Step

The first evaluator is in `main.c`. It walks the generated AST and converts:

- `CommandPart` into `argc` / `argv`
- `Pipeline` into the arrays expected by `execute_pipeline`
- `Redirection` into file descriptors for `dup2`

The existing command registry remains the execution backend. Current support:

- simple commands: `echo hello`, `ls Makefile`
- pipelines: `echo hello | wc`
- top-level redirection: `echo hello > out.txt`, `cat | head -n 1 < Makefile`
- semicolon-separated jobs: `pwd ; echo done`

Per-command redirection in the middle of a pipeline, such as
`cat < Makefile | head -n 1`, is not in the grammar yet. Use top-level
redirection form for now: `cat | head -n 1 < Makefile`.
