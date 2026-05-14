#ifndef CMD_DIRNAME_H
#define CMD_DIRNAME_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_dirname_spec;

int dirname_run(int argc, char **argv);
void dirname_print_usage(FILE *out);
void register_dirname_command(void);

#endif
