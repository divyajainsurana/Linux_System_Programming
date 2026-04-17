
# AGENTS.md

## 📌 Project Overview

This repository contains small Linux CLI utilities written in C.
The goal is to build simple, efficient, and portable command-line tools while following standard system programming practices.

---

## 🎯 Objectives

* Build CLI tools (e.g., `mydate`)
* Practice Linux system programming concepts
* Maintain clean, readable, and modular C code
* Follow POSIX-compatible standards where possible

---

## 🛠️ Technology Stack

* Language: C
* Environment: Linux / macOS terminal
* Compiler: gcc
* Tools: Git, VS Code

---

## 🤖 Agent Responsibilities

AI agents working on this repository should:

* Generate clean and minimal C code
* Follow existing code style and structure
* Add meaningful comments where necessary
* Avoid unnecessary complexity
* Prefer standard libraries (`stdio.h`, `time.h`, etc.)

---

## ⚠️ Constraints & Rules

* Do NOT use unsafe functions (e.g., unchecked memory operations)
* Do NOT introduce external dependencies unless explicitly required
* Keep binaries lightweight and efficient
* Ensure code compiles with `gcc` without warnings
* Maintain portability across Unix-like systems

---

## 🧩 CLI Design Guidelines

* Support basic flags (e.g., `-h` for help)
* Provide clear usage messages
* Keep command behavior predictable
* Follow common Linux CLI conventions

---

## 🧪 Example Tasks

* Create a CLI command to print local date
* Add `-h` help flag to commands
* Extend CLI tools with additional options
* Improve argument parsing

---

## 📂 File Structure

* `*.c` → Source files
* Compiled binaries → Should not be committed (use `.gitignore`)
* Documentation → Markdown files (`README.md`, `AGENTS.md`)

---

## 🚀 Future Improvements

* Add argument parsing using `getopt()`
* Support multiple CLI utilities
* Add unit tests (optional)
* Improve error handling

---

## 📖 Notes

This project is intended for learning and experimentation.
Keep implementations simple, clear, and aligned with system-level programming best practices.
