#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PROJECT_OLLAMA="$SCRIPT_DIR/../tools/Ollama.app/Contents/Resources/ollama"
OLLAMA_BIN="${OLLAMA_BIN:-ollama}"
MODEL="${OLLAMA_MODEL:-qwen2.5:3b}"
OLLAMA_HOST="${OLLAMA_HOST:-http://127.0.0.1:11434}"
OLLAMA_TIMEOUT="${OLLAMA_TIMEOUT:-20}"

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

python3 - "$OLLAMA_HOST" "$MODEL" "$1" "$SCRIPT_DIR" "$OLLAMA_TIMEOUT" <<'PY'
import json
import os
import re
import socket
import subprocess
import sys
import urllib.error
import urllib.request

host, model, request, script_dir, timeout_text = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5]
try:
    timeout = float(timeout_text)
except ValueError:
    timeout = 20.0
unsafe = set(";&|<>`$()\r\n")

def load_shell_catalog():
    builtins = {"help", "version", "exit", "quit"}
    commands = []
    shell_path = os.path.join(script_dir, "busybox_shell")
    if not os.path.exists(shell_path):
        return builtins, commands, shell_path
    try:
        raw = subprocess.check_output([shell_path, "help", "--json"], text=True, timeout=5)
        data = json.loads(raw)
    except Exception:
        return builtins, commands, shell_path

    for item in data.get("commands", []):
        name = item.get("name")
        if name:
            builtins.add(name)
            commands.append(name)
    for item in data.get("builtins", []):
        name = item.get("name")
        if name:
            builtins.add(name)
    return builtins, commands, shell_path

def compact_help(text):
    lines = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        lines.append(line)
    return " ".join(lines)

def load_command_reference(shell_path, commands):
    sections = []
    for name in commands:
        try:
            raw = subprocess.check_output([shell_path, name, "-h"], text=True, timeout=5)
        except Exception:
            continue
        help_text = compact_help(raw)
        if help_text:
            sections.append(f"{name}: {help_text}")
    return "\n".join(sections)

allowed, commands, shell_path = load_shell_catalog()
allowed_text = ", ".join(sorted(allowed))
command_reference = load_command_reference(shell_path, commands)

def request_mentions_command():
    words = re.findall(r"[A-Za-z0-9_.-]+", request.lower())
    for word in reversed(words):
        if word in commands:
            return word
    return None

mentioned_command = request_mentions_command()

prompt = f"""Return exactly one busybox_shell command for the request.
Allowed first words: {allowed_text}
No markdown. No explanation. No shell operators.
Use this command reference to choose flags and command names:
{command_reference}

Examples:
list files => ls
list file names in json format => ls --json
list files in reverse order => ls -r
list hidden files => ls -a
list files with details => ls -l
list files recursively => ls -R
list files sorted by size => ls -S
list newest files => ls -t
print hello => echo hello
help of ls => help ls
what does mkdir do => help mkdir
what is the version of ls => ls --version
show ls version => ls --version
what does --json parameter do in ls => ls -h --json
explain ls --json => ls -h --json
create directory named demo => mkdir demo
Request: {request}
Command:"""

payload = {
    "model": model,
    "prompt": prompt,
    "stream": False,
    "options": {
        "temperature": 0,
        "top_p": 0.1,
        "num_predict": 32,
    },
}

url = host.rstrip("/") + "/api/generate"
data = json.dumps(payload).encode("utf-8")
req = urllib.request.Request(url, data=data, headers={"Content-Type": "application/json"})

try:
    with urllib.request.urlopen(req, timeout=timeout) as response:
        body = json.loads(response.read().decode("utf-8"))
except urllib.error.URLError as exc:
    print(f"Ollama request failed or timed out after {timeout:g}s: {exc}", file=sys.stderr)
    sys.exit(1)
except (TimeoutError, socket.timeout):
    print(f"Ollama request timed out after {timeout:g}s", file=sys.stderr)
    sys.exit(1)

text = body.get("response", "")
for line in text.splitlines():
    line = line.strip().strip("`")
    line = re.sub(r"^(command|answer)\s*:\s*", "", line, flags=re.IGNORECASE).strip()
    if not line or line.startswith("```"):
        continue
    if any(ch in unsafe for ch in line):
        continue
    first = line.split()[0] if line.split() else ""
    if mentioned_command and first == "version":
        continue
    if first in allowed:
        print(line)
        break
else:
    print("help")
PY
