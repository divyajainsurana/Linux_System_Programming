# Session 9 Homework: AiShell Socket Service

## What was implemented

`busybox_shell` now includes a `serve` command that starts a line-based TCP
service endpoint for allowlisted shell services.

Supported request methods:

- `initialize`
- `list_tools`
- `call_tool TOOL [ARGS...]`

Arguments are simple space-separated tokens. Key/value arguments use `key:value`
syntax, for example `path:.`.

## Build

```sh
cd busybox_shell
make
```

The normal build uses the checked-in generated BNFC parser files. The
`bnfc_parser` target is still available when BNFC is installed.

## Run the server

```sh
./busybox_shell serve --host 127.0.0.1 --port 9000 --timeout 5 \
  --max-requests 8 --allow get_time,list_files,delete_older_than_days,ls,pwd,mkdir
```

The same settings can be provided with a config file:

```ini
host=127.0.0.1
port=9000
timeout=5
max_requests=8
allow=get_time,list_files,delete_older_than_days,ls,pwd,mkdir
```

Then run:

```sh
./busybox_shell serve --config service.conf
```

## Test with the RPC client

```sh
./busybox_shell rpc --host 127.0.0.1 --port 9000 initialize
./busybox_shell rpc --host 127.0.0.1 --port 9000 list_tools
./busybox_shell rpc --host 127.0.0.1 --port 9000 call_tool get_time
./busybox_shell rpc --host 127.0.0.1 --port 9000 call_tool list_files path:.
./busybox_shell rpc --host 127.0.0.1 --port 9000 call_tool delete_older_than_days path:. days:30
./busybox_shell rpc --host 127.0.0.1 --port 9000 call_tool pwd
```

`delete_older_than_days` defaults to dry-run mode. Actual deletion requires both
`dry_run:false` and `confirm:DELETE`.

```sh
./busybox_shell rpc --host 127.0.0.1 --port 9000 call_tool delete_older_than_days \
  path:. days:30 dry_run:false confirm:DELETE
```

## Acceptance checks

- Server accepts TCP connections and handles one or more line-based requests.
- Server closes each client connection cleanly after EOF or `max_requests`.
- Timeouts are applied with `SO_RCVTIMEO` and `SO_SNDTIMEO`.
- Configuration changes behavior through `--allow` or `allow=` in config files.
- Disabled tools return a structured error instead of running.
- `delete_older_than_days` validates `days`, scans only one directory level, and
  requires explicit confirmation before deleting.
- Malformed or unknown requests return a structured error.
- Arguments are validated and shell metacharacters are rejected.
- The server never executes arbitrary remote shell strings; it only dispatches
  registered, allowlisted shell commands.

## Verification performed

Built successfully with:

```sh
make
```

Runtime checks performed on local test ports:

```sh
./busybox_shell serve --host 127.0.0.1 --port 19090 --timeout 2 \
  --max-requests 4 --allow get_time,list_files,ls,pwd,mkdir
./busybox_shell rpc --host 127.0.0.1 --port 19090 initialize
./busybox_shell rpc --host 127.0.0.1 --port 19090 list_tools
./busybox_shell rpc --host 127.0.0.1 --port 19090 call_tool pwd
./busybox_shell rpc --host 127.0.0.1 --port 19090 call_tool list_files path:.
./busybox_shell rpc --host 127.0.0.1 --port 19090 nope
```

Allowlist behavior was checked with:

```sh
./busybox_shell serve --host 127.0.0.1 --port 19091 --timeout 2 \
  --max-requests 3 --allow pwd
./busybox_shell rpc --host 127.0.0.1 --port 19091 list_tools
./busybox_shell rpc --host 127.0.0.1 --port 19091 call_tool list_files path:.
./busybox_shell rpc --host 127.0.0.1 --port 19091 call_tool pwd
```

Expected result: only `pwd` is advertised, `list_files` is rejected as disabled,
and `pwd` runs successfully.
