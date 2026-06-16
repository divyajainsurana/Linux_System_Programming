# Week 10 Submission: BusyBox Shell / AiShell

This submission is a C-based BusyBox-style shell that grows across Sessions 1
through 10 of the course. The active shell path is implemented in C. The Week 10
natural-language and agent demos do not require Python helpers.

## Build

```sh
cd busybox_shell
make
```

If BNFC is not installed, the generated C parser files are already present, so
the main shell still builds with:

```sh
gcc -Wall -Wextra -std=c11 -pthread -o busybox_shell \
  main.c cmd_*.c registry.c register_all_commands.c json_utils.c \
  bnfc/Absyn.c bnfc/Buffer.c bnfc/Lexer.c bnfc/Parser.c argtable3/src/*.c
```

Run the interactive shell:

```sh
./busybox_shell
```

Run the full verification suite:

```sh
./test.sh
```

## What Was Implemented

| Course area | Implementation | Example |
|---|---|---|
| Command anatomy | Commands use `cmd_spec_t`, `run`, `print_usage`, and registry registration. | `help ls`, `help --json` |
| Filesystem tools | Built-ins for file and path operations. | `ls -l`, `cat Makefile`, `cp a b`, `rm file` |
| BusyBox shell | One executable dispatches many commands. | `./busybox_shell echo hello` |
| Package metadata | `pkg` command and command catalog/help metadata. | `pkg`, `pkg --json` |
| Process execution | External commands run through `fork()` and `execvp()`. | `date`, `/bin/echo external` |
| Pipelines | Commands are connected using process pipes. | `echo hello world \| wc -w` |
| Jobs/signals | Background jobs, `jobs`, `fg`, `bg`, `kill`, and signal messages. | `sleep 30 &`, `kill -15 %1` |
| Threads | `pthread_create`, `pthread_join`, mutex, race, and signal demos. | `threads --mode mutex -n 2` |
| BNFC parser | Grammar parses commands, pipes, redirection, variables, backticks, and simple `if`. | `x=5`, `echo $x` |
| RPC client | C socket client command with timeout and line protocol. | `rpc call_tool get_time` |
| Socket service | C server endpoint with tool discovery and safe tool calls. | `serve --host 127.0.0.1 --port 9000` |
| Natural-language mode | C deterministic mapper with confirmation gate. | `@ list files` |
| Agent-style mode | C `@ agent` mapper suggests `rpc` tool calls. | `@ agent list tools` |

## Registered Commands

```text
ls, localdate, cat, pkg, pwd, wc, touch, mkdir, rmdir, echo,
whoami, clear, id, uname, head, tail, cp, mv, rm, dirname, du,
procinfo, threads, rpc, serve
```

Shell-only built-ins:

```text
help, exit, quit, jobs, fg, bg, kill, shellpid
```

## Command Anatomy Snippet

Each command exposes one command spec:

```c
cmd_spec_t cmd_threads_spec = {
    .name = "threads",
    .summary = "run a POSIX threads demo",
    .long_help = "Start POSIX worker threads, join them, and print their results.",
    .run = threads_run,
    .print_usage = threads_print_usage,
};
```

All commands are registered in `register_all_commands.c`:

```c
void register_all_builtin_commands(void)
{
    register_ls_command();
    register_threads_command();
    register_rpc_command();
    register_serve_command();
}
```

## Process, Jobs, And Signals Demo

Start the shell:

```sh
./busybox_shell
```

Show the shell process:

```sh
shellpid
```

Create and inspect a background process:

```sh
sleep 30 &
jobs -l
```

Send a graceful termination signal:

```sh
kill -15 %1
jobs -l
```

The shell prints the signal name:

```text
sent SIGTERM (15) to job %1 pid 12345
```

Force kill another job:

```sh
sleep 30 &
kill -9 %2
jobs -l
```

The implementation uses:

```c
pid = fork();
execvp(argv[0], argv);
waitpid(pid, &status, 0);
kill(-pid, signal_number);
```

## Threads Demo

Create worker threads:

```sh
threads -n 4
```

This is a teaching implementation: each thread computes its square and the main
process prints a shared sum.

```text
thread 1 -> 1
thread 2 -> 4
thread 3 -> 9
thread 4 -> 16
sum 30
```

Race condition demo:

```sh
threads --mode race -n 2
```

The race mode intentionally reads the shared counter, pauses, and writes it
back without a lock:

```c
before = *job->counter;
nanosleep(&(struct timespec){0, 1000000L}, NULL);
*job->counter = before + 1;
```

Typical result:

```text
expected counter 20
actual counter 10
race mode has no mutex; result may lose updates
```

Correct mutex version:

```sh
threads --mode mutex -n 2
```

The mutex protects the critical section:

```c
pthread_mutex_lock(job->counter_lock);
before = *job->counter;
(*job->counter)++;
after = *job->counter;
pthread_mutex_unlock(job->counter_lock);
```

Typical result:

```text
expected counter 20
actual counter 20
mutex protected shared counter
```

Signal plus thread shutdown demo:

```sh
threads --mode signal -n 2 --ticks 30 &
jobs -l
kill -15 %1
fg %1
```

## BNFC Grammar Demo

The shell uses generated BNFC C files from `busybox_shell/bnfc`.

Variable assignment and expansion:

```sh
x=5
echo $x
```

Backtick subcommand:

```sh
x=`echo hi`
echo $x
```

Simple if statement:

```sh
if echo cond then echo yes fi
```

Pipeline:

```sh
echo one two three | wc -w
```

Redirection:

```sh
echo saved > out.txt
cat out.txt
rm out.txt
```

## RPC Client And C Socket Service

Terminal 1:

```sh
cd busybox_shell
./busybox_shell serve --host 127.0.0.1 --port 9000 --timeout 5
```

Terminal 2:

```sh
cd busybox_shell
./busybox_shell rpc initialize
./busybox_shell rpc list_tools
./busybox_shell rpc call_tool get_time
./busybox_shell rpc call_tool list_files path:.
```

The protocol is line-based:

```text
initialize
list_tools
call_tool get_time
call_tool list_files path:.
```

Timeouts prevent the shell from hanging:

```sh
./busybox_shell rpc --timeout 2 call_tool get_time
```

For a bounded test/demo server that exits after one client:

```sh
./busybox_shell serve --once --host 127.0.0.1 --port 9000
```

## HTTP Fetch Tool

The C service includes an `http_get` tool. It validates the URL and calls
`curl` through a bounded command with a timeout and byte limit.

Start service:

```sh
./busybox_shell serve --host 127.0.0.1 --port 9000 --allow http_get,get_time,list_files
```

Call fetch-style tool:

```sh
./busybox_shell rpc call_tool http_get url:https://example.com limit:512 timeout:5
```

## Natural Language And Agent Mode

Natural-language command suggestion:

```sh
./busybox_shell
@ list files with details
```

Expected behavior:

```text
AI suggestion: ls -l
Run it? [y/N]
```

C-only agent-style tool suggestion:

```sh
@ agent list tools
@ agent get time
@ agent list files
```

Expected behavior:

```text
Agent tool suggestion: rpc list_tools
Run it? [y/N]
```

The shell validates suggestions and never executes them without confirmation in
interactive mode.

## Verification Checklist

```sh
make
./busybox_shell --version
./busybox_shell help
./busybox_shell help --json
./busybox_shell echo hello
./busybox_shell /bin/echo external-ok
./busybox_shell echo one two three "|" wc -w
./busybox_shell threads --mode race -n 2
./busybox_shell threads --mode mutex -n 2
./busybox_shell rpc --protocol
./test.sh
```

## Notes And Limits

- The submitted shell implementation is C-based.
- BNFC generated C parser files are included so the shell can build without
  regenerating grammar files.
- The `http_get` service tool depends on the system `curl` executable.
- The `@` mode is deterministic and local for this submission. It demonstrates
  the confirmation gate and command-catalog safety without requiring a Python
  LLM helper.
