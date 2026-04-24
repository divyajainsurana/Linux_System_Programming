# 🛠️ CLI Utilities in C (mysh)

This project implements a modular command-line application in C using a **uniform command architecture**.

Each command follows a standard structure using:

* `cmd_spec_t` (command specification)
* `argtable3` for CLI parsing
* a central command **registry system**

---

## 🚀 Build & Run

### Build

```bash
make
```

### Run

```bash
./myshell <command> [options]
```

---

## 📌 Available Commands

---

### 🟡 `localdate`

Prints the current local date.

#### Features

* Displays date in `YYYY-MM-DD` format
* Supports `-h` for help

#### Usage

```bash
./myshell localdate
./myshell localdate -h
```

---

### 🔵 `ls`

Lists directory contents.

#### Features

* Lists files in current directory (default)
* `-a` → includes hidden files
* Supports custom path input
* `-h` → help

#### Usage

```bash
./myshell ls
./myshell ls -a
./myshell ls /tmp
./myshell ls -h
```

---

## 🧠 Architecture

Each command follows a consistent structure:

* `cmd_<name>.c` → command implementation
* `run()` → main execution logic
* `print_usage()` → help/usage output
* `cmd_spec_t` → metadata + function pointers

All commands are registered using a central registry:

```text
register_command()
find_command()
for_each_command()
```

The main dispatcher:

* reads command name
* looks it up in registry
* executes corresponding `run()`

---

## 📂 Project Structure

```text
.
├── main.c
├── cmd_spec.h
├── registry.c
├── register_all_commands.c
│
├── cmd_localdate.c
├── cmd_localdate.h
├── cmd_ls.c
├── cmd_ls.h
│
├── argtable3/
│   └── src/
│       ├── *.c
│       └── *.h
│
├── Makefile
├── README.md
└── PROMPTS.md
```

---

## ⚙️ Technologies Used

* **C (C11 standard)**
* **argtable3** → CLI argument parsing
* **POSIX APIs** → directory handling (`ls`)
* **Makefile** → build system

---

## 🎯 Key Concepts Demonstrated

* Modular command design
* Function pointers in C
* CLI parsing using `argtable3`
* Command registry pattern
* Separation of concerns (logic vs interface)

---

## 🚫 Files Not Included in Repo

```text
*.o
myshell
.DS_Store
```

---

## 💡 Notes

* `argtable3` is vendored locally to avoid dependency issues
* Commands are designed to be easily extendable
* Structure aligns with real-world CLI tool design

---

## 🚀 Future Improvements

* Add more commands (`wc`, `cat`)
* Support multiple arguments in `ls`
* Improve output formatting
* Add error handling enhancements

---

## 👤 Author

* Divya Jain
