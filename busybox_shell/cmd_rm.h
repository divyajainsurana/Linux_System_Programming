#ifndef CMD_RM_H
#define CMD_RM_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_rm_spec;

int rm_run(int argc, char **argv);
void rm_print_usage(FILE *out);
void register_rm_command(void);

#endif
