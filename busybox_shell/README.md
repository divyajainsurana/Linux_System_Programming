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

## JSON Support

The commands with JSON output are:

```sh
./busybox_shell ls --json
./busybox_shell pkg --json
```

`ls --json` prints directory entries as JSON objects. `pkg --json` prints
metadata about the shell and the registered built-in commands.

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
./busybox_shell cat README.md
./busybox_shell pwd
./busybox_shell wc README.md
./busybox_shell touch notes.txt
./busybox_shell mkdir -p tmp/demo
./busybox_shell rmdir tmp/demo
./busybox_shell echo hello world
./busybox_shell whoami
./busybox_shell id
./busybox_shell uname -a
./busybox_shell head -n 5 README.md
./busybox_shell tail -n 5 README.md
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
```

## Author

Divya Jain
