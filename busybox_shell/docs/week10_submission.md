# Week 10 Final Submission: BusyBox Shell / AiShell

This branch contains the final working C implementation of the course shell
project. The main submission is under `busybox_shell/`. The shell is a
BusyBox-style command runner with built-in utilities, BNFC grammar parsing,
process/job control, POSIX thread demos, a C socket/RPC service, and an
OpenRouter-backed AI command planner implemented from C.

No Python helper is required for the submitted shell path.

## Quick Start

Build the shell:

```sh
cd busybox_shell
make
```

Run one command directly:

```sh
./busybox_shell echo hello
./busybox_shell ls -l
./busybox_shell pwd
```

Run the interactive shell:

```sh
./busybox_shell
```

Run all verification tests:

```sh
./test.sh
```

Expected result:

```text
Total test cases: 179
All tests passed!
```

## Week-By-Week Work Completed

| Week / topic | What was implemented in the shell | Demo commands |
|---|---|---|
| Week 1: Linux/Git setup | Project organized as a Git branch-based Linux System Programming submission. | `git branch --show-current` |
| Week 2: Command anatomy | Commands use a shared `cmd_spec_t` structure with name, summary, usage, and run function. | `help`, `help ls`, `help --json` |
| Week 3: Shell basics | Interactive shell loop, direct command execution, built-in dispatch, and external command fallback. | `./busybox_shell`, `date`, `/bin/echo external` |
| Week 4: Package/metadata idea | `pkg` command displays package metadata and registered command catalog. | `pkg`, `pkg --json` |
| Week 5: Processes | External commands run using `fork()` and `execvp()`. Pipelines use `pipe()` and `dup2()`. | `date`, `echo one two \| wc -w` |
| Week 6: Threads/signals/jobs | Background jobs, `jobs`, `fg`, `bg`, `kill`, `shellpid`, POSIX threads, race and mutex demos. | `sleep 30 &`, `kill -15 %1`, `threads --mode mutex -n 2` |
| Week 7/8: BNFC grammar | BNFC-generated C parser handles commands, pipelines, redirection, variables, backticks, and simple `if`. | `x=5`, `echo $x`, ``x=`echo hi` ``, `if echo cond then echo yes fi` |
| Week 8/9: Socket client/server | `rpc` client and `serve` command implement a local line-based tool protocol and optional HTTP mode. | `serve --port 9000`, `rpc list_tools` |
| Week 9/10: AI shell | Natural-language `@` interface, C OpenRouter client, deterministic fallback, and `@ agent` RPC suggestions. | `@ list files`, `@ agent get time` |
| Week 10 final polish | C-only active path, root submission guide, tests, safer AI prompts, and final branch cleanup. | `./test.sh` |

## Source Layout

Important files:

```text
busybox_shell/main.c                    shell loop, BNFC dispatch, jobs, OpenRouter C client
busybox_shell/cmd_*.c                   command implementations
busybox_shell/cmd_rpc.c                 line-based RPC client
busybox_shell/cmd_serve.c               local socket/HTTP service
busybox_shell/cmd_threads.c             POSIX thread, race, mutex, signal demos
busybox_shell/register_all_commands.c   command registry setup
busybox_shell/bnfc/Grammar.cf           shell grammar
busybox_shell/bnfc/*.c, *.h             generated BNFC C parser files
busybox_shell/test.sh                   full verification suite
WEEK10_SUBMISSION.md                    final submission guide
```

## Registered Commands

Built-in registered commands:

```text
ls, localdate, cat, pkg, pwd, wc, touch, mkdir, rmdir, echo,
whoami, clear, id, uname, head, tail, cp, mv, rm, dirname, du,
procinfo, threads, rpc, serve
```

Shell-only commands:

```text
help, exit, quit, jobs, fg, bg, kill, shellpid
```

## Command Anatomy

Commands are registered through a shared command specification:

```c
cmd_spec_t cmd_threads_spec = {
    .name = "threads",
    .summary = "run a POSIX threads demo",
    .long_help = "Start POSIX worker threads, join them, and print their results.",
    .run = threads_run,
    .print_usage = threads_print_usage,
};
```

The registry connects each command to the shell:

```c
void register_all_builtin_commands(void)
{
    register_ls_command();
    register_threads_command();
    register_rpc_command();
    register_serve_command();
}
```

Try:

```sh
./busybox_shell help
./busybox_shell help threads
./busybox_shell help --json
```

## Basic Shell Commands

Run these directly:

```sh
./busybox_shell pwd
./busybox_shell localdate
./busybox_shell whoami
./busybox_shell id
./busybox_shell uname -a
./busybox_shell echo hello world
./busybox_shell echo -e 'a\nb'
./busybox_shell ls -a -l
./busybox_shell cat README.md
./busybox_shell head -n 3 README.md
./busybox_shell tail -n 3 README.md
./busybox_shell wc README.md
./busybox_shell du .
./busybox_shell du README.md
```

Note: the custom built-in `du` supports simple paths such as `du .` and
`du README.md`. It does not implement `-s` or `-h`, and `du /` can hit macOS
permission errors while walking protected directories.

## Files And Directories Demo

Inside `busybox_shell/`:

```sh
./busybox_shell mkdir demo_dir
./busybox_shell touch demo_dir/a.txt
./busybox_shell echo hello > demo_dir/a.txt
./busybox_shell cat demo_dir/a.txt
./busybox_shell cp demo_dir/a.txt demo_dir/b.txt
./busybox_shell mv demo_dir/b.txt demo_dir/c.txt
./busybox_shell dirname demo_dir/c.txt
./busybox_shell du demo_dir/c.txt
./busybox_shell rm demo_dir/a.txt demo_dir/c.txt
./busybox_shell rmdir demo_dir
```

## External Commands And Pipelines

External commands use `fork()` and `execvp()` when no registered built-in
matches:

```sh
./busybox_shell date
./busybox_shell /bin/echo external command works
```

Pipelines use `pipe()` and `dup2()`:

```sh
./busybox_shell
echo hello world | wc -w
echo one two three | wc
```

The implementation path is:

```text
parse command line
  -> split pipeline commands
  -> pipe()
  -> fork()
  -> child dup2() stdin/stdout
  -> child execvp() or built-in command
  -> parent waitpid()
```

## Process, Jobs, And Signals

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

Send `SIGTERM`:

```sh
kill -15 %1
jobs -l
```

Force kill with `SIGKILL`:

```sh
sleep 30 &
jobs -l
kill -9 %2
jobs -l
```

The shell prints signal names, for example:

```text
sent SIGTERM (15) to job %1 pid 12345
sent SIGKILL (9) to job %2 pid 12346
```

Important process APIs used:

```c
pid = fork();
execvp(argv[0], argv);
waitpid(pid, &status, 0);
kill(-pid, signal_number);
```

## POSIX Threads Demo

Create worker threads:

```sh
./busybox_shell threads -n 4
```

Run as JSON:

```sh
./busybox_shell threads --json -n 2
```

Countdown demo:

```sh
./busybox_shell threads --mode countdown -n 2
```

Race condition demo:

```sh
./busybox_shell threads --mode race -n 2
```

Race mode intentionally updates a shared counter without a mutex:

```c
before = *job->counter;
nanosleep(&(struct timespec){0, 1000000L}, NULL);
*job->counter = before + 1;
```

Mutex-correct version:

```sh
./busybox_shell threads --mode mutex -n 2
```

Mutex mode protects the critical section:

```c
pthread_mutex_lock(job->counter_lock);
before = *job->counter;
(*job->counter)++;
after = *job->counter;
pthread_mutex_unlock(job->counter_lock);
```

Signal-aware thread shutdown:

```sh
./busybox_shell threads --mode signal -n 2 --heartbeats 2
```

## BNFC Grammar Features

The shell grammar lives in:

```text
busybox_shell/bnfc/Grammar.cf
```

Generated parser files are already included, so the project builds even if
BNFC is not installed.

Try these inside `./busybox_shell`:

```sh
x=5
echo $x
```

Backtick subcommand:

```sh
x=`echo hi`
echo $x
```

Simple `if then fi`:

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

## RPC Client And Socket Service

The shell includes a local line-based RPC service.

Terminal 1:

```sh
cd busybox_shell
./busybox_shell serve --host 127.0.0.1 --port 9000
```

Terminal 2:

```sh
cd busybox_shell
./busybox_shell rpc initialize
./busybox_shell rpc list_tools
./busybox_shell rpc call_tool get_time
./busybox_shell rpc call_tool list_files path:.
```

The line protocol is:

```text
initialize
list_tools
call_tool get_time
call_tool list_files path:.
```

Timeouts prevent the shell from hanging on network/server issues:

```sh
./busybox_shell rpc --timeout 2 call_tool get_time
```

For a one-request demo server:

```sh
./busybox_shell serve --once --host 127.0.0.1 --port 9000
```

Important: `rpc` talks to the line-based server. Do not start the server with
`--http` for `rpc`.

## HTTP Mode

HTTP mode is for `curl` demos, not for the `rpc` command.

Terminal 1:

```sh
cd busybox_shell
./busybox_shell serve --http --host 127.0.0.1 --port 9000
```

Terminal 2:

```sh
curl http://127.0.0.1:9000/tools
curl 'http://127.0.0.1:9000/call?tool=get_time'
curl 'http://127.0.0.1:9000/call?tool=list_files&arg=path:.'
```

The service also has an `http_get` tool:

```sh
./busybox_shell rpc call_tool http_get url:https://example.com limit:512 timeout:5
```

## Natural Language Interface

Start the shell:

```sh
./busybox_shell
```

Try local suggestions:

```sh
@ list files
@ where am I
@ print hello
@ show first lines of README.md
@ count words in README.md
@ check disk size
@ size of README.md
```

Examples:

```text
@ check disk size
AI suggestion: du .

@ size of README.md
AI suggestion: du README.md
```

The shell validates suggestions before running them and asks for confirmation:

```text
Run it? [y/N]
```

## OpenRouter AI Agent In C

OpenRouter is implemented in C inside `busybox_shell/main.c`. There is no
Python helper in the submitted shell path.

Main functions:

```c
static int openrouter_nl_to_command(const char *request,
                                    char *command,
                                    size_t command_size);

static int write_openrouter_request_file(const char *request,
                                         const char *model,
                                         char *path,
                                         size_t path_size);

static int parse_openrouter_command(const char *response,
                                    char *command,
                                    size_t command_size);
```

Flow:

```text
@ natural-language request
  -> C builds OpenRouter JSON request
  -> C calls curl with OPENROUTER_API_KEY
  -> C parses choices[0].message.content
  -> C cleans the returned command
  -> C validates the command against shell safety rules
  -> shell asks Run it? [y/N]
```

Run with OpenRouter:

```sh
cd busybox_shell
export OPENROUTER_API_KEY="your_api_key"
export OPENROUTER_MODEL="qwen/qwen-2.5-7b-instruct"
make
./busybox_shell
```

Then:

```sh
@ show hidden files with details
@ create a directory called reports
@ count words in README.md
```

If `OPENROUTER_API_KEY` is not set, or when commands are piped into the shell
for automated tests, `@` uses the deterministic C fallback mapper.

## Agent-Style RPC Suggestions

The `@ agent` mode maps natural-language tool requests to `rpc` commands.

Inside `./busybox_shell`:

```sh
@ agent list tools
@ agent get time
@ agent list files
@ agent fetch https://example.com
```

Examples:

```text
Agent tool suggestion: rpc list_tools
Agent tool suggestion: rpc call_tool get_time
Agent tool suggestion: rpc call_tool list_files path:.
Agent tool suggestion: rpc call_tool http_get url:https://example.com
```

## Full Demo Script

Use this sequence for a compact professor demo:

```sh
cd busybox_shell
make
./busybox_shell --version
./busybox_shell help
./busybox_shell pkg
./busybox_shell echo hello
./busybox_shell /bin/echo external-ok
```

Then interactive:

```sh
./busybox_shell
shellpid
sleep 30 &
jobs -l
kill -15 %1
threads --mode mutex -n 2
x=5
echo $x
x=`echo hi`
echo $x
if echo cond then echo yes fi
@ check disk size
exit
```

RPC demo:

```sh
# Terminal 1
cd busybox_shell
./busybox_shell serve --host 127.0.0.1 --port 9000
```

```sh
# Terminal 2
cd busybox_shell
./busybox_shell rpc initialize
./busybox_shell rpc list_tools
./busybox_shell rpc call_tool get_time
```

Final verification:

```sh
cd busybox_shell
./test.sh
```

## Notes And Limits

- The active submission path is C.
- OpenRouter support is implemented from C using a `curl` subprocess.
- The `http_get` service tool also depends on the system `curl` executable.
- BNFC generated C parser files are included, so the shell builds without
  regenerating grammar files.
- `du -sh /` is not supported by the custom built-in `du`; use `du .`,
  `du README.md`, or an external system command such as `/usr/bin/du -sh /`.
- Session folders are intentionally not part of the submitted `week10` branch;
  the branch contains the workable shell project and submission guide.
