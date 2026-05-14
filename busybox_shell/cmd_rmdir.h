#ifndef CMD_RMDIR_H
#define CMD_RMDIR_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_rmdir_spec;

int rmdir_run(int argc, char **argv);
void rmdir_print_usage(FILE *out);
void register_rmdir_command(void);

#endif
