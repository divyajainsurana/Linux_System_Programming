#ifndef CMD_CP_H
#define CMD_CP_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_cp_spec;

int cp_run(int argc, char **argv);
void cp_print_usage(FILE *out);
void register_cp_command(void);

#endif
