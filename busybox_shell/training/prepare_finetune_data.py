#!/usr/bin/env python3

import argparse
import json


SYSTEM_PROMPT = (
    "You translate natural-language requests into exactly one safe "
    "busybox_shell command. Return only the command. Do not explain. "
    "Do not use shell operators, pipes, redirects, command substitution, "
    "semicolons, or markdown."
)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", default="../training_data.jsonl")
    parser.add_argument("--output", default="mysh_train_chat.jsonl")
    args = parser.parse_args()

    count = 0
    with open(args.input, "r", encoding="utf-8") as src, open(
        args.output, "w", encoding="utf-8"
    ) as dst:
        for line in src:
            if not line.strip():
                continue
            item = json.loads(line)
            record = {
                "messages": [
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": item["prompt"]},
                    {"role": "assistant", "content": item["completion"]},
                ]
            }
            dst.write(json.dumps(record, ensure_ascii=False) + "\n")
            count += 1

    print(f"Wrote {count} chat examples to {args.output}")


if __name__ == "__main__":
    main()
