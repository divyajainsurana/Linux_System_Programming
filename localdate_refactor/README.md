# localdate

`localdate` is a small C command-line utility that prints the current local date.

## Features

- Prints local date in `YYYY-MM-DD` format
- Supports `-h` / `--help`
- Uses the command module structure defined by `cmd_spec_t`
- Uses `argtable3` as the single source of truth for CLI help and parsing

## Files

```text
cmd_spec.h          Shared command specification type
cmd_localdate.h     localdate command module header
cmd_localdate.c     localdate command implementation
localdate_main.c    Small standalone binary wrapper
```

## Build

You need `argtable3` installed.

```bash
gcc localdate_main.c cmd_localdate.c -largtable3 -o localdate
```

If you are building inside the shell project, compile `cmd_localdate.c` with the shell registry implementation that provides `register_command()`.

## Usage

```bash
./localdate
./localdate -h
./localdate --help
```

Example output:

```text
Local Date: 2026-04-23
```
