# BusyBox-Style Shell in C

This project implements a small BusyBox-inspired shell in C. One executable,
`busybox_shell`, contains many Unix-style commands and dispatches them through
a shared command registry.

Each command follows the same anatomy:

```c
int command_run(int argc, char **argv);
void command_print_usage(FILE *out);

cmd_spec_t cmd_command_spec = {
    .name = "...",
    .summary = "...",
    .long_help = "...",
    .run = command_run,
    .print_usage = command_print_usage,
};
```

## Build

```sh
make
```

Run a command directly:

```sh
./busybox_shell <command> [options]
```

Run the interactive shell:

```sh
./busybox_shell
```

Inside the shell:

```text
busybox_shell> help
busybox_shell> ls
busybox_shell> echo hello shell
busybox_shell> pkg
busybox_shell> exit
```

Interactive sessions support command history:

```text
Up arrow      recall older commands
Down arrow    move forward through recalled commands
Tab           complete command names or file paths
Left/Right    move the cursor while editing
Backspace     edit the current command
```

History is saved between sessions in:

```text
~/.busybox_shell_history
```

Tab completion uses command names for the first word and file/path names for
later words:

```text
busybox_shell> ec<Tab>        completes to echo
busybox_shell> cat READ<Tab>  completes to README.md when it is unique
busybox_shell> c<Tab>         shows matches such as cat, clear, and cp
```

## Natural-Language `@` Interface

Interactive input that starts with `@` is treated as a natural-language shell
request:

```text
busybox_shell> @ show today's date
AI suggestion: localdate
Run it? [y/N] y
Local Date: 2026-05-14
```

The shell can connect to an external LLM helper through `MYSH_LLM_HELPER`.
The helper receives one prompt argument and should print exactly one
`busybox_shell` command:

```sh
MYSH_LLM_HELPER="./my_llm_helper.sh" ./busybox_shell
```

An Ollama helper is included. Install Ollama, pull a local model, then start
the shell with the helper:

```sh
ollama pull qwen2.5:1.5b
MYSH_LLM_HELPER="./ollama_llm_helper.sh" ./busybox_shell
```

To use a different local model:

```sh
OLLAMA_MODEL="mistral" MYSH_LLM_HELPER="./ollama_llm_helper.sh" ./busybox_shell
```

If Ollama was installed locally in this repository, use:

```sh
cd busybox_shell
HOME="$PWD/../tools/ollama-home" \
OLLAMA_MODELS="$PWD/../tools/ollama-models" \
MYSH_LLM_HELPER="./ollama_llm_helper.sh" \
./busybox_shell
```

The shell validates the suggested command before running it. Only registered
commands and shell built-ins are accepted. In interactive mode, suggestions are
shown first and require confirmation. In non-interactive mode, suggestions are
shown but not executed.

If no helper is configured, the shell uses a tiny demo fallback for common
requests such as listing files, showing the current directory, printing the
date, showing the current user, and showing system information.

## Commands

Built-in commands currently registered:

```text
ls          list directory contents
localdate   print the current local date
cat         concatenate and print files
pkg         manage shell packages
pwd         print the current working directory
wc          count lines, words, and bytes
touch       create files or update timestamps
mkdir       create directories
rmdir       remove empty directories
echo        print text to standard output
whoami      print the current username
clear       clear the terminal screen
id          print user and group identifiers
uname       print system information
head        print the first lines of files
tail        print the last lines of files
cp          copy files
mv          move or rename files
rm          remove files
dirname     print directory portion of paths
du          show disk usage
```

Every command supports help through either:

```sh
./busybox_shell <command> -h
./busybox_shell help <command>
```

Every registered command also supports version output:

```sh
./busybox_shell <command> --version
./busybox_shell <command> --version --json
```

Shell-level version output is available with:

```sh
./busybox_shell --version
./busybox_shell --version --json
```

Command help includes these shared features too:

```sh
./busybox_shell help ls
./busybox_shell ls -h
```

Both show the command-specific options plus a common options section for
`--version`, `--version --json`, and JSON help.

## JSON Support

All built-in commands support `--json` output. Examples:

```sh
./busybox_shell ls --json
./busybox_shell pkg --json
./busybox_shell id --json
./busybox_shell uname --json
./busybox_shell wc --json README.md
./busybox_shell cat --json README.md
./busybox_shell head --json -n 5 README.md
./busybox_shell tail --json -n 5 README.md
./busybox_shell cp --json source.txt dest.txt
```

Data-oriented commands return their data as JSON fields. File-content commands
such as `cat`, `head`, and `tail` wrap escaped file contents in JSON. Commands
that mutate files, such as `cp`, `mv`, `rm`, `touch`, `mkdir`, and `rmdir`,
return a success/status object when `--json` is provided.

Help can also be returned as JSON:

```sh
./busybox_shell help --json
./busybox_shell help ls --json
./busybox_shell ls -h --json
```

`help --json` lists shell built-ins and registered commands. `help <command>
--json` and `<command> -h --json` return JSON metadata for a single command.

## Package Manager

The `pkg` command is both a built-in command and a tiny package manager for
installing extra external commands.

Supported subcommands:

```sh
./busybox_shell pkg build <src-dir> <output-tar>
./busybox_shell pkg install <tar-file>
./busybox_shell pkg list
./busybox_shell pkg remove <name>
```

Package files install under:

```text
~/.mysh/pkgs/<name>-<version>/
```

Executable symlinks are created under:

```text
~/.mysh/bin/
```

Installed packages are tracked in:

```text
~/.mysh/pkgdb.txt
```

## Minimal Package Format

A package is a `.tar.gz` archive containing at least:

```text
pkg.json
bin/<executable>
```

Example source directory:

```text
hello_pkg/
├── pkg.json
└── bin/
    └── hello
```

Example `pkg.json`:

```json
{
  "name": "hello",
  "version": "1.0.0",
  "description": "Tiny test command",
  "files": ["bin/hello"]
}
```

Example executable:

```sh
#!/bin/sh
echo "hello from package"
```

Build and install:

```sh
./busybox_shell pkg build hello_pkg hello-1.0.0.tar.gz
./busybox_shell pkg install hello-1.0.0.tar.gz
./busybox_shell pkg list
~/.mysh/bin/hello
./busybox_shell pkg remove hello
```

For safe testing, use a temporary home directory:

```sh
HOME="$PWD/test_home" ./busybox_shell pkg install hello-1.0.0.tar.gz
```

That keeps test installs under `./test_home/.mysh` instead of your real home
directory.

## Manual Test Examples

```sh
./busybox_shell localdate
./busybox_shell ls -a
./busybox_shell ls -l
./busybox_shell ls -R tmp
./busybox_shell ls -S
./busybox_shell ls -t
./busybox_shell ls -r
./busybox_shell ls --color
./busybox_shell cat README.md
./busybox_shell cat -n README.md
./busybox_shell cat -b README.md
./busybox_shell cat -s README.md
./busybox_shell cat -E README.md
./busybox_shell pwd
./busybox_shell pwd -L
./busybox_shell pwd -P
./busybox_shell wc README.md
./busybox_shell touch notes.txt
./busybox_shell touch -c maybe-missing.txt
./busybox_shell touch -v notes.txt
./busybox_shell touch -t 202501010000 notes.txt
./busybox_shell mkdir -p tmp/demo
./busybox_shell mkdir -v tmp/demo2
./busybox_shell mkdir -m 700 private-dir
./busybox_shell mkdir --dry-run tmp/planned
./busybox_shell rmdir tmp/demo
./busybox_shell echo hello world
./busybox_shell echo -e 'hello\nworld'
./busybox_shell echo -E 'hello\nworld'
./busybox_shell whoami
./busybox_shell id
./busybox_shell uname -a
./busybox_shell head -n 5 README.md
./busybox_shell head -c 40 README.md
./busybox_shell head -v -n 5 README.md
./busybox_shell tail -n 5 README.md
./busybox_shell tail -c 40 README.md
./busybox_shell tail -v -n 5 README.md
./busybox_shell tail -f log.txt
./busybox_shell dirname a/b/c
./busybox_shell du README.md
```

File operation example:

```sh
./busybox_shell touch manual_test.txt
./busybox_shell cp manual_test.txt manual_copy.txt
./busybox_shell mv manual_copy.txt manual_moved.txt
./busybox_shell rm manual_test.txt manual_moved.txt
```

## Automated Tests

Run:

```sh
./test.sh
```

or:

```sh
make test
```

The test script checks the built-in commands and also creates a temporary
`hello` package to verify:

```text
pkg build -> pkg install -> pkg list -> installed executable -> pkg remove
```

## Architecture

Core files:

```text
main.c                    interactive shell and command dispatch
cmd_spec.h                shared command interface
registry.c                command registry implementation
register_all_commands.c   built-in command registration
cmd_<name>.c/.h           individual command modules
argtable3/                vendored argument parsing library
```

The shell flow is:

```text
read input
load/save interactive history
translate @ natural-language requests
split into argv
handle help/exit/quit
find command in registry
call command->run(argc, argv)
```

## Notes

This is BusyBox-style in the educational sense: one binary contains many
commands. Real BusyBox also supports symlink dispatch and heavy compile-time
configuration; this project keeps the design simpler and easier to study.

Current limitations:

```text
cp copies one source file to one destination
rm removes files but does not recursively delete directories
pkg uses simple string-based pkg.json parsing, not a full JSON parser
package-installed commands are linked into ~/.mysh/bin but are not yet
automatically executed by the interactive shell unless run by path
command history supports simple quoted text only through the existing
whitespace-based command splitter
tab completion follows the same whitespace-based parsing, so paths containing
spaces are not completed as quoted shell words yet
the @ interface validates only the first suggested command word, so complex
shell syntax is intentionally not supported
```

## Author

Divya Jain
