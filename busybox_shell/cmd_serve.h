#ifndef CMD_SERVE_H
#define CMD_SERVE_H

#include <stdio.h>

int serve_run(int argc, char **argv);
void serve_print_usage(FILE *out);
void register_serve_command(void);

#endif
