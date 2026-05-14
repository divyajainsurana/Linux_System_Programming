#ifndef CMD_CLEAR_H
#define CMD_CLEAR_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_clear_spec;

int clear_run(int argc, char **argv);
void clear_print_usage(FILE *out);
void register_clear_command(void);

#endif
