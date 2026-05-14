#ifndef CMD_TAIL_H
#define CMD_TAIL_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_tail_spec;

int tail_run(int argc, char **argv);
void tail_print_usage(FILE *out);
void register_tail_command(void);

#endif
