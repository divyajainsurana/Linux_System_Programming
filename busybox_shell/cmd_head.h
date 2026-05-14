#ifndef CMD_HEAD_H
#define CMD_HEAD_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_head_spec;

int head_run(int argc, char **argv);
void head_print_usage(FILE *out);
void register_head_command(void);

#endif
