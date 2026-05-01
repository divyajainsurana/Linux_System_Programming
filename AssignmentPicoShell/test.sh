#!/bin/sh

set -e

cd "$(dirname "$0")"

make clean
make

printf 'Testing foreground command...\n'
printf '/bin/echo hello\nquit\n' | ./psh -p | grep 'hello'

printf 'Testing background job and jobs builtin...\n'
printf './myspin 1 &\njobs\nquit\n' | ./psh -p | grep 'Running ./myspin 1 &'

printf 'Testing missing command error...\n'
printf './does-not-exist\nquit\n' | ./psh -p | grep './does-not-exist: Command not found'

printf 'Testing bg/fg validation...\n'
printf 'bg\nfg abc\nquit\n' | ./psh -p | grep 'bg command requires PID or %jobid argument'
printf 'bg\nfg abc\nquit\n' | ./psh -p | grep 'fg: argument must be a PID or %jobid'

printf 'All PicoShell tests passed!\n'
