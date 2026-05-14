#!/bin/bash

set -e

#make clean
#make

printf "\033[1mTesting localdate...\033[0m\n"
./busybox_shell localdate | grep -E "Local Date: [0-9]{4}-[0-9]{2}-[0-9]{2}"
./busybox_shell localdate -h | grep "Usage:"

printf "\033[1mTesting ls...\033[0m\n"
./busybox_shell ls | grep "Makefile"
./busybox_shell ls -h | grep "Usage:"
./busybox_shell ls /tmp > /dev/null
./busybox_shell ls -h --json | grep "\"name\":\"Makefile\""

printf "\033[1mTesting cat...\033[0m\n"
./busybox_shell cat Makefile | grep "TARGET = busybox_shell"
./busybox_shell cat -h | grep "Usage:"

printf "\033[1mTesting pkg...\033[0m\n"
./busybox_shell pkg | grep "Package: busybox_shell"
./busybox_shell pkg | grep "pkg - manage shell packages"
./busybox_shell pkg --json | grep "\"name\":\"busybox_shell\""
./busybox_shell pkg -h | grep "Usage:"
./busybox_shell pkg -h | grep "pkg install"

printf "\033[1mTesting package manager...\033[0m\n"
rm -rf test_home test_pkg_src test_pkg.tar.gz
mkdir -p test_pkg_src/bin
cat > test_pkg_src/pkg.json <<'PKGJSON'
{
  "name": "hello",
  "version": "1.0.0",
  "description": "Tiny test command",
  "files": ["bin/hello"]
}
PKGJSON
cat > test_pkg_src/bin/hello <<'HELLO'
#!/bin/sh
echo "hello from package"
HELLO
chmod +x test_pkg_src/bin/hello
HOME="$PWD/test_home" ./busybox_shell pkg build test_pkg_src test_pkg.tar.gz
test -f test_pkg.tar.gz
HOME="$PWD/test_home" ./busybox_shell pkg install test_pkg.tar.gz | grep "Installed hello 1.0.0"
HOME="$PWD/test_home" ./busybox_shell pkg list | grep "hello 1.0.0"
test -L test_home/.mysh/bin/hello
test "$(test_home/.mysh/bin/hello)" = "hello from package"
HOME="$PWD/test_home" ./busybox_shell pkg remove hello | grep "Removed hello 1.0.0"
HOME="$PWD/test_home" ./busybox_shell pkg list | grep "No packages installed."
rm -rf test_home test_pkg_src test_pkg.tar.gz

printf "\033[1mTesting echo...\033[0m\n"
./busybox_shell echo hello world | grep "^hello world$"
test "$(./busybox_shell echo -n hello)" = "hello"
./busybox_shell echo -h | grep "Usage:"

printf "\033[1mTesting whoami...\033[0m\n"
./busybox_shell whoami | grep "$(whoami)"
./busybox_shell whoami -h | grep "Usage:"
./busybox_shell whoami -h | grep "Print the username of the current user."

printf "\033[1mTesting system commands...\033[0m\n"
./busybox_shell id | grep "uid="
./busybox_shell id -h | grep "Print the current user and group identifiers."
./busybox_shell uname | grep "$(uname -s)"
./busybox_shell uname -a | grep "$(uname -m)"
./busybox_shell uname -h | grep "Usage:"
./busybox_shell clear > /dev/null
./busybox_shell clear -h | grep "Clear the terminal screen."

printf "\033[1mTesting file/path commands...\033[0m\n"
printf "one\ntwo\nthree\nfour\n" > test_lines.tmp
./busybox_shell head -n 2 test_lines.tmp | grep "^two$"
./busybox_shell tail -n 2 test_lines.tmp | grep "^three$"
./busybox_shell cp test_lines.tmp test_copy.tmp
./busybox_shell cat test_copy.tmp | grep "^four$"
./busybox_shell mv test_copy.tmp test_move.tmp
test -f test_move.tmp
./busybox_shell dirname a/b/c | grep "^a/b$"
./busybox_shell du test_move.tmp | grep "test_move.tmp"
./busybox_shell rm test_lines.tmp test_move.tmp
test ! -f test_lines.tmp
test ! -f test_move.tmp
./busybox_shell rm -f missing_file.tmp
./busybox_shell head -h | grep "Usage:"
./busybox_shell tail -h | grep "Usage:"
./busybox_shell cp -h | grep "Usage:"
./busybox_shell mv -h | grep "Usage:"
./busybox_shell rm -h | grep "Usage:"
./busybox_shell dirname -h | grep "Usage:"
./busybox_shell du -h | grep "Usage:"

printf "\033[1mTesting interactive shell...\033[0m\n"
printf "localdate\npkg\nhelp ls\nexit\n" | ./busybox_shell | grep "Local Date:"
printf "localdate\npkg\nhelp ls\nexit\n" | ./busybox_shell | grep "Package: busybox_shell"
printf "localdate\npkg\nhelp ls\nexit\n" | ./busybox_shell | grep "Usage: ls"

printf "\033[1mTesting invalid command...\033[0m\n"
if ./busybox_shell unknown; then
    echo "FAIL: unknown command should fail"
    exit 1
else
    echo "PASS: unknown command failed correctly"
fi

printf "\033[1mTesting invalid ls flag...\033[0m\n"
if ./busybox_shell ls -b > /dev/null 2>&1; then
    echo "FAIL: ls -b should fail"
    exit 1
else
    echo "PASS: ls -b failed correctly"
fi

echo "All tests passed!"
