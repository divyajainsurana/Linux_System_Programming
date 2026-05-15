#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SHELL_BIN="${SHELL_BIN:-$SCRIPT_DIR/busybox_shell}"
OUTPUT="${1:-$SCRIPT_DIR/training_data.jsonl}"

if [ ! -x "$SHELL_BIN" ]; then
    echo "busybox_shell binary not found. Run make first, or set SHELL_BIN." >&2
    exit 1
fi

python3 - "$SHELL_BIN" "$OUTPUT" <<'PY'
import json
import subprocess
import sys

shell_bin, output_path = sys.argv[1], sys.argv[2]

def run_shell(*args):
    return subprocess.check_output([shell_bin, *args], text=True, timeout=5)

def add(examples, seen, prompt, completion, source):
    prompt = " ".join(prompt.split())
    completion = " ".join(completion.split())
    key = (prompt.lower(), completion)
    if key in seen:
        return
    seen.add(key)
    examples.append({
        "prompt": prompt,
        "completion": completion,
        "source": source,
    })

def command_names():
    data = json.loads(run_shell("help", "--json"))
    return [item["name"] for item in data.get("commands", []) if item.get("name")]

def option_flags(help_text):
    flags = []
    for raw_line in help_text.splitlines():
        line = raw_line.strip()
        if not line.startswith("-"):
            continue
        flag_part = line.split(None, 1)[0]
        for part in flag_part.split(","):
            flag = part.strip()
            if flag:
                flags.append(flag)
    return flags

examples = []
seen = set()
commands = command_names()

for name in commands:
    add(examples, seen, f"show help for {name}", f"{name} -h", "generic-help")
    add(examples, seen, f"how do I use {name}", f"{name} -h", "generic-help")
    add(examples, seen, f"show help for {name} in json", f"{name} -h --json", "json-help")
    add(examples, seen, f"explain {name} as json", f"{name} -h --json", "json-help")
    add(examples, seen, f"what is the version of {name}", f"{name} --version", "version")
    add(examples, seen, f"show {name} version", f"{name} --version", "version")

    try:
        help_text = run_shell(name, "-h")
    except Exception:
        help_text = ""

    for flag in option_flags(help_text):
        if flag in ("-h", "--help"):
            continue
        add(examples, seen, f"what does {flag} do in {name}", f"{name} -h --json", "option-help")
        add(examples, seen, f"explain {name} {flag}", f"{name} -h --json", "option-help")
        add(examples, seen, f"show {name} {flag} parameter in json", f"{name} -h --json", "option-help")

manual = [
    ("list files", "ls"),
    ("list file names in json format", "ls --json"),
    ("list files in reverse order", "ls -r"),
    ("list hidden files", "ls -a"),
    ("list hidden files with details", "ls -a -l"),
    ("list files recursively", "ls -R"),
    ("list files sorted by size", "ls -S"),
    ("list newest files", "ls -t"),
    ("where am I", "pwd"),
    ("show current directory", "pwd"),
    ("print hello", "echo hello"),
    ("say hello world", "echo hello world"),
    ("create directory named reports", "mkdir reports"),
    ("make folder called logs", "mkdir logs"),
    ("remove directory old", "rmdir old"),
    ("who am I", "whoami"),
    ("show system information", "uname -a"),
    ("show my uid and gid", "id"),
    ("clear the screen", "clear"),
    ("count words in README.md", "wc README.md"),
    ("show first lines of README.md", "head README.md"),
    ("show last lines of README.md", "tail README.md"),
    ("show disk usage of README.md", "du README.md"),
    ("read file README.md", "cat README.md"),
]

for prompt, completion in manual:
    add(examples, seen, prompt, completion, "manual-template")

with open(output_path, "w", encoding="utf-8") as out:
    for example in examples:
        out.write(json.dumps(example, ensure_ascii=False) + "\n")

print(f"Wrote {len(examples)} examples to {output_path}")
PY
