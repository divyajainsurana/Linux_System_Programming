#ifndef CMD_THREADS_H
#define CMD_THREADS_H

#include <stdio.h>

int threads_run(int argc, char **argv);
void threads_print_usage(FILE *out);
void register_threads_command(void);

#endif
