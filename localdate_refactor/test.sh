#!/bin/bash

set -e

#make clean
#make

printf "\033[1mTesting localdate...\033[0m\n"
./myshell localdate | grep -E "Local Date: [0-9]{4}-[0-9]{2}-[0-9]{2}"
./myshell localdate -h | grep "Usage:"

printf "\033[1mTesting ls...\033[0m\n"
./myshell ls | grep "Makefile"
./myshell ls -h | grep "Usage:"
./myshell ls /tmp > /dev/null

printf "\033[1mTesting invalid command...\033[0m\n"
if ./myshell unknown; then
    echo "FAIL: unknown command should fail"
    exit 1
else
    echo "PASS: unknown command failed correctly"
fi

printf "\033[1mTesting invalid ls flag...\033[0m\n"
if ./myshell ls -b > /dev/null 2>&1; then
    echo "FAIL: ls -b should fail"
    exit 1
else
    echo "PASS: ls -b failed correctly"
fi

echo "All tests passed!"
