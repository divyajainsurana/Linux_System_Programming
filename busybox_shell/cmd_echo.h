#ifndef CMD_ECHO_H
#define CMD_ECHO_H

#include <stdio.h>
#include "cmd_spec.h"

extern cmd_spec_t cmd_echo_spec;

int echo_run(int argc, char **argv);
void echo_print_usage(FILE *out);
void register_echo_command(void);

#endif
