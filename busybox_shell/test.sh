#!/bin/bash

set -e

#make clean
#make

printf "\033[1mTesting version output...\033[0m\n"
./busybox_shell --version | grep "busybox_shell 1.0.0"
./busybox_shell ls --version | grep "ls (busybox_shell) 1.0.0"
./busybox_shell pkg --version | grep "pkg (busybox_shell) 1.0.0"
./busybox_shell rm --version --json | grep "\"version\":\"1.0.0\""
./busybox_shell help | grep "Common features:"
./busybox_shell help ls | grep "show command version and exit"

printf "\033[1mTesting localdate...\033[0m\n"
./busybox_shell localdate | grep -E "Local Date: [0-9]{4}-[0-9]{2}-[0-9]{2}"
./busybox_shell localdate -h | grep "Usage:"
./busybox_shell localdate --json | grep "\"date\":"

printf "\033[1mTesting ls...\033[0m\n"
./busybox_shell ls | grep "Makefile"
./busybox_shell ls -h | grep "Usage:"
./busybox_shell ls /tmp > /dev/null
./busybox_shell ls --json | grep "\"name\":\"Makefile\""
./busybox_shell ls -h --json | grep "\"summary\":\"list directory contents\""
./busybox_shell help ls --json | grep "\"description\":\"List files in a directory"
./busybox_shell help --json | grep "\"commands\":"
./busybox_shell ls -l Makefile | grep "Makefile"
./busybox_shell ls -S | grep "Makefile"
./busybox_shell ls -t | grep "Makefile"
./busybox_shell ls -r | grep "Makefile"
mkdir -p test_ls_recursive/subdir
touch test_ls_recursive/subdir/nested.txt
./busybox_shell ls -R test_ls_recursive | grep "nested.txt"
rm -rf test_ls_recursive

printf "\033[1mTesting cat...\033[0m\n"
./busybox_shell cat Makefile | grep "TARGET = busybox_shell"
./busybox_shell cat -h | grep "Usage:"
./busybox_shell cat --json Makefile | grep "\"content\":"
./busybox_shell wc --json Makefile | grep "\"bytes\":"
printf "a\n\n\nb\n" > cat_features.tmp
./busybox_shell cat -n cat_features.tmp | grep "1"
./busybox_shell cat -b cat_features.tmp | grep "2"
./busybox_shell cat -s cat_features.tmp | grep "^$"
./busybox_shell cat -E cat_features.tmp | grep 'a\$'
rm -f cat_features.tmp

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
./busybox_shell echo -e 'a\nb' | grep "^b$"
./busybox_shell echo -E 'a\nb' | grep 'a\\nb'
./busybox_shell echo -h | grep "Usage:"
./busybox_shell echo --json hello world | grep "\"text\":\"hello world\""

printf "\033[1mTesting whoami...\033[0m\n"
./busybox_shell whoami | grep "$(whoami)"
./busybox_shell whoami -h | grep "Usage:"
./busybox_shell whoami -h | grep "Print the username of the current user."
./busybox_shell whoami --json | grep "\"username\":"

printf "\033[1mTesting system commands...\033[0m\n"
./busybox_shell id | grep "uid="
./busybox_shell id -h | grep "Print the current user and group identifiers."
./busybox_shell id --json | grep "\"uid\":"
./busybox_shell uname | grep "$(uname -s)"
./busybox_shell uname -a | grep "$(uname -m)"
./busybox_shell uname -h | grep "Usage:"
./busybox_shell uname --json | grep "\"sysname\":"
./busybox_shell clear > /dev/null
./busybox_shell clear -h | grep "Clear the terminal screen."
./busybox_shell clear --json | grep "\"cleared\":true"

printf "\033[1mTesting file/path commands...\033[0m\n"
./busybox_shell pwd --json | grep "\"cwd\":"
./busybox_shell pwd -P | grep "$PWD"
./busybox_shell pwd -L | grep "$PWD"
printf "one\ntwo\nthree\nfour\n" > test_lines.tmp
./busybox_shell head -n 2 test_lines.tmp | grep "^two$"
./busybox_shell tail -n 2 test_lines.tmp | grep "^three$"
./busybox_shell head -c 3 test_lines.tmp | grep "one"
./busybox_shell tail -c 5 test_lines.tmp | grep "four"
./busybox_shell head -v -n 1 test_lines.tmp | grep "==> test_lines.tmp <=="
./busybox_shell tail -v -n 1 test_lines.tmp | grep "==> test_lines.tmp <=="
./busybox_shell head --json -n 1 test_lines.tmp | grep "\"command\":\"head\""
./busybox_shell tail --json -n 1 test_lines.tmp | grep "\"command\":\"tail\""
./busybox_shell cp test_lines.tmp test_copy.tmp
./busybox_shell cp --json test_lines.tmp test_copy_json.tmp | grep "\"copied\":true"
./busybox_shell cat test_copy.tmp | grep "^four$"
./busybox_shell mv test_copy.tmp test_move.tmp
./busybox_shell mv --json test_copy_json.tmp test_move_json.tmp | grep "\"moved\":true"
test -f test_move.tmp
./busybox_shell dirname a/b/c | grep "^a/b$"
./busybox_shell dirname --json a/b/c | grep "\"dirname\":\"a/b\""
./busybox_shell du test_move.tmp | grep "test_move.tmp"
./busybox_shell du --json test_move.tmp | grep "\"kilobytes\":"
./busybox_shell rm --json test_move_json.tmp | grep "\"success\":true"
./busybox_shell rm test_lines.tmp test_move.tmp
test ! -f test_lines.tmp
test ! -f test_move.tmp
test ! -f test_move_json.tmp
./busybox_shell rm -f missing_file.tmp
./busybox_shell touch --json touch_json.tmp | grep "\"success\":true"
./busybox_shell touch -c missing_no_create.tmp
test ! -f missing_no_create.tmp
./busybox_shell touch -v touch_verbose.tmp | grep "touched touch_verbose.tmp"
./busybox_shell touch -t 202501010000 touch_time.tmp
./busybox_shell rm touch_json.tmp
./busybox_shell mkdir --dry-run mkdir_dry_run_dir 2>&1 | grep "would create mkdir_dry_run_dir"
test ! -d mkdir_dry_run_dir
./busybox_shell mkdir -m 700 mkdir_mode_dir
test -d mkdir_mode_dir
./busybox_shell rmdir mkdir_mode_dir
./busybox_shell mkdir -v mkdir_verbose_dir | grep "created mkdir_verbose_dir"
./busybox_shell rmdir mkdir_verbose_dir
./busybox_shell mkdir --json mkdir_json_dir | grep "\"success\":true"
./busybox_shell rmdir --json mkdir_json_dir | grep "\"success\":true"
./busybox_shell rm touch_verbose.tmp touch_time.tmp
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
