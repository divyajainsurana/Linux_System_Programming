#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_OLLAMA="$SCRIPT_DIR/../tools/Ollama.app/Contents/Resources/ollama"
OLLAMA_BIN="${OLLAMA_BIN:-ollama}"
MODEL="${OLLAMA_MODEL:-qwen2.5:1.5b}"
OLLAMA_HOST="${OLLAMA_HOST:-http://127.0.0.1:11434}"

if [ -x "$PROJECT_OLLAMA" ]; then
    OLLAMA_BIN="$PROJECT_OLLAMA"
elif ! command -v "$OLLAMA_BIN" >/dev/null 2>&1; then
    echo "ollama command not found. Install Ollama and run: ollama pull $MODEL" >&2
    exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "python3 is required for the Ollama helper" >&2
    exit 1
fi

if [ "$#" -lt 1 ]; then
    echo "usage: $0 '<natural-language prompt>'" >&2
    exit 1
fi

if ! "$OLLAMA_BIN" list >/dev/null 2>&1; then
    echo "Ollama server is not running. Start it with: ollama serve" >&2
    exit 1
fi

python3 - "$OLLAMA_HOST" "$MODEL" "$1" <<'PY'
import json
import sys
import urllib.request

host, model, request = sys.argv[1], sys.argv[2], sys.argv[3]

prompt = f"""You translate natural-language requests into exactly one command for this educational busybox_shell.

Allowed commands:
ls, localdate, cat, pkg, pwd, wc, touch, mkdir, rmdir, echo, whoami, clear, id, uname, head, tail, cp, mv, rm, dirname, du, help, version.

Rules:
- Return only the command.
- Do not use markdown.
- Do not explain.
- Do not use pipes, redirection, command substitution, semicolons, &&, ||, or backticks.
- Prefer safe read-only commands unless the user clearly asks for file changes.
- If unsure, return: help

Examples:
User request: list files
ls

User request: show the current directory
pwd

User request: what is today's date
localdate

User request: who am I
whoami

User request: show system info
uname -a

User request: show help
help

User request: create a directory named demo
mkdir demo

User request: make a folder called reports
mkdir reports

User request: remove directory old
rmdir old

User request: {request}
"""

payload = {
    "model": model,
    "prompt": prompt,
    "stream": False,
    "options": {
        "temperature": 0,
        "top_p": 0.1,
    },
}

url = host.rstrip("/") + "/api/generate"
data = json.dumps(payload).encode("utf-8")
req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})

with urllib.request.urlopen(req, timeout=120) as response:
    body = json.loads(response.read().decode("utf-8"))

text = body.get("response", "").strip()
for line in text.splitlines():
    line = line.strip().strip("`")
    if line and not line.startswith("```"):
        print(line)
        break
else:
    print("help")
PY
