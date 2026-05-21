#!/bin/bash

set -e

#make clean
#make

SECTION_NO=0
TEST_NO=0
TOTAL_TESTS=0

CYAN="\033[1;36m"
GREEN="\033[1;32m"
RED="\033[1;31m"
RESET="\033[0m"

show_usage() {
    echo "Usage: $0 [all|pkg|pkg-demo]"
    echo "  all       run the full test suite (default)"
    echo "  pkg       run only the pkg checks and package manager demo"
    echo "  pkg-demo  run only the package build/install/list/remove demo"
}

print_section() {
    SECTION_NO=$((SECTION_NO + 1))
    TEST_NO=0
    printf "\n${CYAN}[%02d] %s${RESET}\n" "$SECTION_NO" "$1"
    printf "${CYAN}%-8s %-46s %-8s %-58s${RESET}\n" "Case" "Test" "Result" "BusyBox output"
    printf "${CYAN}%-8s %-46s %-8s %-58s${RESET}\n" "--------" "----------------------------------------------" "--------" "----------------------------------------------------------"
}

compact_output() {
    local text="$1"

    text="$(printf "%s" "$text" | tr '\n\t' '  ' | tr -cd '\11\12\15\40-\176' | sed 's/  */ /g; s/^ //; s/ $//')"
    if [ -z "$text" ]; then
        text="-"
    elif [ "${#text}" -gt 58 ]; then
        text="${text:0:55}..."
    fi

    printf "%s" "$text"
}

preview_command() {
    local cmd="$1"
    local preview

    preview="$(printf "%s" "$cmd" | sed -E 's/[[:space:]]*\|[[:space:]]*grep[[:space:]]+-[A-Za-z]*[[:space:]]+.*$//')"
    preview="$(printf "%s" "$preview" | sed 's/^ *//; s/ *$//')"

    case "$preview" in
        ""|test\ *|!\ *|*' pkg build '*|*' pkg install '*|*' pkg remove '*|*' cp '*|*' mv '*|*' rm '*|*' mkdir '*|*' rmdir '*|*' touch '*)
            printf "-"
            return
            ;;
    esac

    if [ "$preview" = "$cmd" ] && ! printf "%s" "$preview" | grep -q '\./busybox_shell'; then
        printf "-"
        return
    fi

    compact_output "$(bash -c "$preview" 2>&1 || true)"
}

run_test() {
    local output

    TEST_NO=$((TEST_NO + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    output="$(preview_command "$2")"
    printf "%-8s %-46s " "$(printf "%02d.%02d" "$SECTION_NO" "$TEST_NO")" "$1"
    if bash -c "$2" > /dev/null 2>&1; then
        printf "${GREEN}%-8s${RESET} %-58s\n" "PASS" "$output"
    else
        printf "${RED}%-8s${RESET} %-58s\n" "FAIL" "$output"
        return 1
    fi
}

run_display_test() {
    local output

    TEST_NO=$((TEST_NO + 1))
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    printf "%-8s %-46s " "$(printf "%02d.%02d" "$SECTION_NO" "$TEST_NO")" "$1"
    if output="$(bash -c "$2")"; then
        printf "${GREEN}%-8s${RESET} %-58s\n" "PASS" "$(compact_output "$output")"
        printf "%s\n" "$output" | sed 's/^/         | /'
    else
        printf "${RED}%-8s${RESET}\n" "FAIL"
        return 1
    fi
}

print_summary() {
    printf "\n${CYAN}Total test cases: %d${RESET}\n" "$TOTAL_TESTS"
}

cleanup_pkg_demo() {
    rm -rf test_home test_pkg_src test_pkg.tar.gz
}

run_pkg_demo() {
    print_section "Testing package manager demo"
    cleanup_pkg_demo
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

    run_test "pkg build creates tarball" \
        'HOME="$PWD/test_home" ./busybox_shell pkg build test_pkg_src test_pkg.tar.gz && test -f test_pkg.tar.gz'
    run_test "pkg install reports installed package" \
        'HOME="$PWD/test_home" ./busybox_shell pkg install test_pkg.tar.gz | grep -q "Installed hello 1.0.0"'
    run_test "pkg list shows installed package" \
        'HOME="$PWD/test_home" ./busybox_shell pkg list | grep -q "hello 1.0.0"'
    run_test "pkg install creates executable symlink" \
        'test -L test_home/.mysh/bin/hello'
    run_test "installed package command runs" \
        'test "$(test_home/.mysh/bin/hello)" = "hello from package"'
    run_test "pkg remove reports removed package" \
        'HOME="$PWD/test_home" ./busybox_shell pkg remove hello | grep -q "Removed hello 1.0.0"'
    run_test "pkg list is empty after removal" \
        'HOME="$PWD/test_home" ./busybox_shell pkg list | grep -q "No packages installed."'

    cleanup_pkg_demo
}

run_pkg_section() {
    print_section "Testing pkg"
    run_display_test "pkg prints busybox package details" './busybox_shell pkg'
    run_test "pkg output includes package name" \
        './busybox_shell pkg | grep -q "Package: busybox_shell"'
    run_test "pkg output includes command summary" \
        './busybox_shell pkg | grep -q "pkg - manage shell packages"'
    run_test "pkg json includes package name" \
        './busybox_shell pkg --json | grep -q "\"name\":\"busybox_shell\""'
    run_test "pkg help prints usage" \
        './busybox_shell pkg -h | grep -q "Usage:"'
    run_test "pkg help includes install usage" \
        './busybox_shell pkg -h | grep -q "pkg install"'
    run_pkg_demo
}

case "${1:-all}" in
    all)
        ;;
    pkg)
        run_pkg_section
        print_summary
        echo "Pkg tests passed!"
        exit 0
        ;;
    pkg-demo|demo-pkg)
        run_pkg_demo
        print_summary
        echo "Package manager demo passed!"
        exit 0
        ;;
    -h|--help|help)
        show_usage
        exit 0
        ;;
    *)
        show_usage
        exit 1
        ;;
esac

print_section "Testing version output"
run_test "busybox_shell version" './busybox_shell --version | grep -q "busybox_shell 1.0.0"'
run_test "ls command version" './busybox_shell ls --version | grep -q "ls (busybox_shell) 1.0.0"'
run_test "pkg command version" './busybox_shell pkg --version | grep -q "pkg (busybox_shell) 1.0.0"'
run_test "rm json version" './busybox_shell rm --version --json | grep -q "\"version\":\"1.0.0\""'
run_test "help shows common features" './busybox_shell help | grep -q "Common features:"'
run_test "command help shows version flag" './busybox_shell help ls | grep -q "show command version and exit"'
run_test "help shows natural-language interface" './busybox_shell help | grep -q "@ <request>"'

print_section "Testing natural-language @ interface"
run_test "@ list files suggests ls" 'printf "@list files\nexit\n" | ./busybox_shell | grep -q "AI suggestion: ls"'
run_test "@ where am I suggests pwd" 'printf "@where am I\nexit\n" | ./busybox_shell | grep -q "AI suggestion: pwd"'
run_test "@ create directory suggests mkdir" 'printf "@create a directory named ai_test_dir\nexit\n" | ./busybox_shell | grep -q "AI suggestion: mkdir ai_test_dir"'
run_test "@ print hello suggests echo" 'printf "@print hello\nexit\n" | ./busybox_shell | grep -q "AI suggestion: echo hello"'
run_test "@ help of ls suggests help ls" 'printf "@help of ls\nexit\n" | ./busybox_shell | grep -q "AI suggestion: help ls"'
run_test "@ what mkdir does suggests help mkdir" 'printf "@what does mkdir do\nexit\n" | ./busybox_shell | grep -q "AI suggestion: help mkdir"'
run_test "@ count words suggests wc" 'printf "@count words in Makefile\nexit\n" | ./busybox_shell | grep -q "AI suggestion: wc Makefile"'
run_test "@ first lines suggests head" 'printf "@show first lines of Makefile\nexit\n" | ./busybox_shell | grep -q "AI suggestion: head Makefile"'
run_test "@ last lines suggests tail" 'printf "@show last lines of Makefile\nexit\n" | ./busybox_shell | grep -q "AI suggestion: tail Makefile"'
run_test "@ json list suggests ls --json" 'printf "@list file names in json format\nexit\n" | ./busybox_shell | grep -q "AI suggestion: ls --json"'
run_test "@ reverse list suggests ls -r" 'printf "@list files in reverse order\nexit\n" | ./busybox_shell | grep -q "AI suggestion: ls -r"'
run_test "@ hidden details suggests ls -a -l" 'printf "@list hidden files with details\nexit\n" | ./busybox_shell | grep -q "AI suggestion: ls -a -l"'

print_section "Testing localdate"
run_test "localdate prints date" './busybox_shell localdate | grep -Eq "Local Date: [0-9]{4}-[0-9]{2}-[0-9]{2}"'
run_test "localdate help prints usage" './busybox_shell localdate -h | grep -q "Usage:"'
run_test "localdate json includes date" './busybox_shell localdate --json | grep -q "\"date\":"'

print_section "Testing ls"
run_test "ls shows Makefile" './busybox_shell ls | grep -q "Makefile"'
run_test "ls help prints usage" './busybox_shell ls -h | grep -q "Usage:"'
run_test "ls accepts /tmp path" './busybox_shell ls /tmp'
run_test "ls json includes Makefile" './busybox_shell ls --json | grep -q "\"name\":\"Makefile\""'
run_test "ls json help includes summary" './busybox_shell ls -h --json | grep -q "\"summary\":\"list directory contents\""'
run_test "help ls json includes description" './busybox_shell help ls --json | grep -q "\"description\":\"List files in a directory"'
run_test "help json includes commands" './busybox_shell help --json | grep -q "\"commands\":"'
run_test "ls -l shows Makefile" './busybox_shell ls -l Makefile | grep -q "Makefile"'
run_test "ls -S sorts and shows Makefile" './busybox_shell ls -S | grep -q "Makefile"'
run_test "ls -t sorts and shows Makefile" './busybox_shell ls -t | grep -q "Makefile"'
run_test "ls -r reverses and shows Makefile" './busybox_shell ls -r | grep -q "Makefile"'
mkdir -p test_ls_recursive/subdir
touch test_ls_recursive/subdir/nested.txt
run_test "ls -R shows nested file" './busybox_shell ls -R test_ls_recursive | grep -q "nested.txt"'
rm -rf test_ls_recursive

print_section "Testing cat"
run_test "cat prints Makefile content" './busybox_shell cat Makefile | grep -q "TARGET = busybox_shell"'
run_test "cat help prints usage" './busybox_shell cat -h | grep -q "Usage:"'
run_test "cat json includes content" './busybox_shell cat --json Makefile | grep -q "\"content\":"'
run_test "wc json includes bytes" './busybox_shell wc --json Makefile | grep -q "\"bytes\":"'
printf "a\n\n\nb\n" > cat_features.tmp
run_test "cat -n numbers lines" './busybox_shell cat -n cat_features.tmp | grep -q "1"'
run_test "cat -b numbers nonblank lines" './busybox_shell cat -b cat_features.tmp | grep -q "2"'
run_test "cat -s squeezes blank lines" './busybox_shell cat -s cat_features.tmp | grep -q "^$"'
run_test "cat -E marks line endings" './busybox_shell cat -E cat_features.tmp | grep -q "a\\$"'
rm -f cat_features.tmp

run_pkg_section

print_section "Testing echo"
run_test "echo prints words" './busybox_shell echo hello world | grep -q "^hello world$"'
run_test "echo -n omits newline" 'test "$(./busybox_shell echo -n hello)" = "hello"'
run_test "echo -e interprets escapes" "./busybox_shell echo -e 'a\\nb' | grep -q '^b$'"
run_test "echo -E keeps escapes literal" "./busybox_shell echo -E 'a\\nb' | grep -q 'a\\\\nb'"
run_test "echo help prints usage" './busybox_shell echo -h | grep -q "Usage:"'
run_test "echo json includes text" './busybox_shell echo --json hello world | grep -q "\"text\":\"hello world\""'

print_section "Testing whoami"
run_test "whoami prints current user" './busybox_shell whoami | grep -q "$(whoami)"'
run_test "whoami help prints usage" './busybox_shell whoami -h | grep -q "Usage:"'
run_test "whoami help prints description" './busybox_shell whoami -h | grep -q "Print the username of the current user."'
run_test "whoami json includes username" './busybox_shell whoami --json | grep -q "\"username\":"'

print_section "Testing system commands"
run_test "id prints uid" './busybox_shell id | grep -q "uid="'
run_test "id help prints description" './busybox_shell id -h | grep -q "Print the current user and group identifiers."'
run_test "id json includes uid" './busybox_shell id --json | grep -q "\"uid\":"'
run_test "uname prints system name" './busybox_shell uname | grep -q "$(uname -s)"'
run_test "uname -a includes machine" './busybox_shell uname -a | grep -q "$(uname -m)"'
run_test "uname help prints usage" './busybox_shell uname -h | grep -q "Usage:"'
run_test "uname json includes sysname" './busybox_shell uname --json | grep -q "\"sysname\":"'
run_test "clear runs" './busybox_shell clear'
run_test "clear help prints description" './busybox_shell clear -h | grep -q "Clear the terminal screen."'
run_test "clear json reports cleared" './busybox_shell clear --json | grep -q "\"cleared\":true"'

print_section "Testing process and thread support"
run_test "pipeline uses process path" './busybox_shell echo alpha beta gamma "|" wc -w | grep -q "3"'
run_test "pipeline counts four words" './busybox_shell echo one two three four "|" wc -w | grep -q "4"'
run_test "pipeline counts echoed bytes" './busybox_shell echo hello "|" wc -c | grep -q "6"'
run_test "pipeline wc json includes word count" './busybox_shell echo alpha beta "|" wc --json | grep -q "\"words\":2"'
run_test "three command pipeline runs" './busybox_shell echo alpha beta gamma "|" wc -w "|" wc -c | grep -q "9"'
run_test "pipeline returns last command failure" '! ./busybox_shell echo hello "|" missing-command'
run_test "binary imports process calls" 'nm -u ./busybox_shell | grep -Eq "(_fork| fork|_execvp| execvp)"'
run_test "procinfo prints process ids" './busybox_shell procinfo | grep -Eq "pid=[0-9]+ ppid=[0-9]+"'
run_test "procinfo json includes pid" './busybox_shell procinfo --json | grep -q "\"pid\":"'
run_test "procinfo can run in pipeline" './busybox_shell procinfo "|" wc -w | grep -q "2"'
run_test "threads command starts workers" './busybox_shell threads -n 3 | grep -q "started 3 threads"'
run_test "threads command joins workers" './busybox_shell threads -n 3 | grep -Eq "thread 3 id [0-9]+ result 9"'
run_test "threads command prints mutex sum" './busybox_shell threads -n 3 | grep -q "sum 14"'
run_test "threads command supports sleep" './busybox_shell threads -n 2 --sleep 1 | grep -q "started 2 threads"'
run_test "threads json includes count" './busybox_shell threads --json -n 2 | grep -q "\"threads\":2"'
run_test "threads json includes sum" './busybox_shell threads --json -n 2 | grep -q "\"sum\":5"'
run_test "threads json includes thread ids" './busybox_shell threads --json -n 2 | grep -q "\"thread_id\":"'
run_test "binary imports pthread calls" 'nm -u ./busybox_shell | grep -Eq "pthread_create|_pthread_create"'

print_section "Testing file/path commands"
run_test "pwd json includes cwd" './busybox_shell pwd --json | grep -q "\"cwd\":"'
run_test "pwd -P prints physical cwd" './busybox_shell pwd -P | grep -q "$PWD"'
run_test "pwd -L prints logical cwd" './busybox_shell pwd -L | grep -q "$PWD"'
printf "one\ntwo\nthree\nfour\n" > test_lines.tmp
run_test "head -n prints second line with two lines" './busybox_shell head -n 2 test_lines.tmp | grep -q "^two$"'
run_test "tail -n prints third line with two lines" './busybox_shell tail -n 2 test_lines.tmp | grep -q "^three$"'
run_test "head -c prints first bytes" './busybox_shell head -c 3 test_lines.tmp | grep -q "one"'
run_test "tail -c prints last bytes" './busybox_shell tail -c 5 test_lines.tmp | grep -q "four"'
run_test "head -v prints filename header" './busybox_shell head -v -n 1 test_lines.tmp | grep -q "==> test_lines.tmp <=="'
run_test "tail -v prints filename header" './busybox_shell tail -v -n 1 test_lines.tmp | grep -q "==> test_lines.tmp <=="'
run_test "head json includes command" './busybox_shell head --json -n 1 test_lines.tmp | grep -q "\"command\":\"head\""'
run_test "tail json includes command" './busybox_shell tail --json -n 1 test_lines.tmp | grep -q "\"command\":\"tail\""'
run_test "cp copies file" './busybox_shell cp test_lines.tmp test_copy.tmp'
run_test "cp json reports copied" './busybox_shell cp --json test_lines.tmp test_copy_json.tmp | grep -q "\"copied\":true"'
run_test "copied file contains last line" './busybox_shell cat test_copy.tmp | grep -q "^four$"'
run_test "mv renames file" './busybox_shell mv test_copy.tmp test_move.tmp'
run_test "mv json reports moved" './busybox_shell mv --json test_copy_json.tmp test_move_json.tmp | grep -q "\"moved\":true"'
run_test "moved file exists" 'test -f test_move.tmp'
run_test "dirname prints parent path" './busybox_shell dirname a/b/c | grep -q "^a/b$"'
run_test "dirname json includes dirname" './busybox_shell dirname --json a/b/c | grep -q "\"dirname\":\"a/b\""'
run_test "du prints file path" './busybox_shell du test_move.tmp | grep -q "test_move.tmp"'
run_test "du json includes kilobytes" './busybox_shell du --json test_move.tmp | grep -q "\"kilobytes\":"'
run_test "rm json removes moved json file" './busybox_shell rm --json test_move_json.tmp | grep -q "\"success\":true"'
run_test "rm removes temp files" './busybox_shell rm test_lines.tmp test_move.tmp'
run_test "removed source file is gone" 'test ! -f test_lines.tmp'
run_test "removed moved file is gone" 'test ! -f test_move.tmp'
run_test "removed json file is gone" 'test ! -f test_move_json.tmp'
run_test "rm -f ignores missing file" './busybox_shell rm -f missing_file.tmp'
run_test "touch json creates file" './busybox_shell touch --json touch_json.tmp | grep -q "\"success\":true"'
run_test "touch -c skips missing file" './busybox_shell touch -c missing_no_create.tmp'
run_test "touch -c did not create file" 'test ! -f missing_no_create.tmp'
run_test "touch -v prints touched file" './busybox_shell touch -v touch_verbose.tmp | grep -q "touched touch_verbose.tmp"'
run_test "touch -t sets timestamp" './busybox_shell touch -t 202501010000 touch_time.tmp'
run_test "rm removes touch json file" './busybox_shell rm touch_json.tmp'
run_test "mkdir dry-run prints action" './busybox_shell mkdir --dry-run mkdir_dry_run_dir 2>&1 | grep -q "would create mkdir_dry_run_dir"'
run_test "mkdir dry-run did not create directory" 'test ! -d mkdir_dry_run_dir'
run_test "mkdir -m creates directory" './busybox_shell mkdir -m 700 mkdir_mode_dir'
run_test "mkdir mode directory exists" 'test -d mkdir_mode_dir'
run_test "rmdir removes mode directory" './busybox_shell rmdir mkdir_mode_dir'
run_test "mkdir -v prints created directory" './busybox_shell mkdir -v mkdir_verbose_dir | grep -q "created mkdir_verbose_dir"'
run_test "rmdir removes verbose directory" './busybox_shell rmdir mkdir_verbose_dir'
run_test "mkdir json reports success" './busybox_shell mkdir --json mkdir_json_dir | grep -q "\"success\":true"'
run_test "rmdir json reports success" './busybox_shell rmdir --json mkdir_json_dir | grep -q "\"success\":true"'
run_test "rm removes touch temp files" './busybox_shell rm touch_verbose.tmp touch_time.tmp'
run_test "head help prints usage" './busybox_shell head -h | grep -q "Usage:"'
run_test "tail help prints usage" './busybox_shell tail -h | grep -q "Usage:"'
run_test "cp help prints usage" './busybox_shell cp -h | grep -q "Usage:"'
run_test "mv help prints usage" './busybox_shell mv -h | grep -q "Usage:"'
run_test "rm help prints usage" './busybox_shell rm -h | grep -q "Usage:"'
run_test "dirname help prints usage" './busybox_shell dirname -h | grep -q "Usage:"'
run_test "du help prints usage" './busybox_shell du -h | grep -q "Usage:"'

print_section "Testing interactive shell"
run_test "interactive localdate prints date" 'printf "localdate\npkg\nhelp ls\nexit\n" | ./busybox_shell | grep -q "Local Date:"'
run_test "interactive pkg prints package" 'printf "localdate\npkg\nhelp ls\nexit\n" | ./busybox_shell | grep -q "Package: busybox_shell"'
run_test "interactive help ls prints usage" 'printf "localdate\npkg\nhelp ls\nexit\n" | ./busybox_shell | grep -q "Usage: ls"'

print_section "Testing invalid command"
run_test "unknown command fails" '! ./busybox_shell unknown'

print_section "Testing invalid ls flag"
run_test "ls -b fails" '! ./busybox_shell ls -b'

print_summary
echo "All tests passed!"
