#!/bin/sh

set -e

cd "$(dirname "$0")"

make clean
make

printf 'Testing pwd...\n'
printf 'pwd\nexit\n' | ./Unix_Shell | grep "$(pwd)"

printf 'Testing external command...\n'
printf 'echo hello\nexit\n' | ./Unix_Shell | grep "hello"

printf 'Testing output redirection...\n'
rm -f shell_test_out.txt
printf 'echo saved > shell_test_out.txt\ncat shell_test_out.txt\nexit\n' | ./Unix_Shell | grep "saved"
rm -f shell_test_out.txt

printf 'Testing pipe...\n'
printf 'printf hello | wc -c\nexit\n' | ./Unix_Shell | grep "5"

printf 'All tests passed!\n'
