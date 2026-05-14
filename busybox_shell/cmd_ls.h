#ifndef CMD_LS_H
#define CMD_LS_H

#include <stdio.h>
#include "cmd_spec.h"

int ls_run(int argc, char **argv);
void ls_print_usage(FILE *out);

extern cmd_spec_t cmd_ls_spec;

void register_ls_command(void);

#endif
