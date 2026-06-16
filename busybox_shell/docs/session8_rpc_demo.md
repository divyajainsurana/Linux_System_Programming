# Session 8 RPC Client Demo

## What Was Implemented

AiShell now has a registered client-facing command:

```sh
./busybox_shell rpc ...
```

The command sends one request line to a TCP service and prints the response. It
is intended as the Session 8 client-side command for MCP-style demos.

The modified C-only agent is:

```text
Session 8_ Sockets Client/example_agent.c
```

It accepts a natural-language prompt, maps it to a small RPC request, sends that
request to the toy RPC server, and prints the result.

## Registry And Help Integration

The command is registered through the normal AiShell command registry:

```text
register_all_builtin_commands()
  -> register_rpc_command()
  -> register_command(&cmd_rpc_spec)
```

It appears in:

```sh
./busybox_shell help
./busybox_shell help rpc
./busybox_shell help --json
```

## Minimal Message Format

The protocol is line-based.

```text
request  = METHOD [ARGS...] "\n"
args     = space-separated tokens
key/value tokens use key:value
response = server-defined text
```

Examples:

```text
initialize
list_tools
call_tool get_time
call_tool list_files path:.
```

Colon syntax is used for key/value arguments because `=` already means variable
assignment in the BNFC shell grammar.

## Demo Commands

Start a compatible local server in one terminal:

```sh
cd "Session 8_ Sockets Client"
make
./toy_rpc_server
```

Run the AiShell RPC command in another terminal:

```sh
cd busybox_shell
./busybox_shell rpc initialize
./busybox_shell rpc list_tools
./busybox_shell rpc call_tool get_time
./busybox_shell rpc call_tool list_files path:.
./busybox_shell rpc --timeout 2 call_tool get_time
```

Run the modified C-only agent:

```sh
cd "Session 8_ Sockets Client"
./example_agent "what time is it"
./example_agent "list files"
./example_agent "show available tools"
```

Expected output examples:

```text
RESULT initialize server=toy_rpc version=1.0
RESULT list_tools get_time echo list_files
RESULT get_time 2026-...
RESULT list_files
```

## AI Work Log

| Step | Work completed |
|---|---|
| 1 | Reviewed the Session 8 notebook requirement for a client-facing command. |
| 2 | Chose a toy line-based RPC client instead of an HTTP client to keep the demo small and C-focused. |
| 3 | Implemented `cmd_rpc.c` and `cmd_rpc.h`. |
| 4 | Registered `rpc` in `register_all_commands.c`. |
| 5 | Added `cmd_rpc.c` to the BusyBox shell `Makefile`. |
| 6 | Defined the line-based message format with colon key/value arguments. |
| 7 | Added a C toy server in `Session 8_ Sockets Client` for live demo testing. |
| 8 | Added `example_agent.c` as the C-only modified agent. |
| 9 | Added `--timeout SECONDS` so network reads/writes do not hang forever. |
| 10 | Added test coverage for help, protocol output, timeout validation, and registry visibility. |

## Verification Steps

Build AiShell:

```sh
cd busybox_shell
make
```

Check registry/help:

```sh
./busybox_shell help | grep rpc
./busybox_shell help rpc
./busybox_shell rpc --protocol
./busybox_shell rpc --timeout 0 initialize
```

Run the demo server:

```sh
cd "../Session 8_ Sockets Client"
make
./toy_rpc_server
```

Run the client command:

```sh
cd ../busybox_shell
./busybox_shell rpc initialize
./busybox_shell rpc list_tools
./busybox_shell rpc call_tool get_time
./busybox_shell rpc --timeout 2 call_tool get_time
```

Timeout behavior:

```text
--timeout SECONDS sets send and receive socket timeouts.
The default is 5 seconds.
Invalid values such as 0 are rejected before connecting.
```

Verify the modified agent:

```sh
cd "../Session 8_ Sockets Client"
./example_agent "what time is it"
./example_agent "list files"
./example_agent "show available tools"
```

Run regression tests:

```sh
./test.sh
```

## Optional Natural-Language Stub

AiShell already has an `@` mode. It can map natural-language requests to shell
commands and then dispatch them through the normal BNFC-backed execution path.
The RPC command can be suggested through that same path later, for example:

```text
@ ask rpc server for time
AI suggestion: rpc call_tool get_time
```
