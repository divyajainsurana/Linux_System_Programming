#ifndef CMD_MKDIR_H
#define CMD_MKDIR_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_mkdir_spec;

int mkdir_run(int argc, char **argv);
void mkdir_print_usage(FILE *out);
void register_mkdir_command(void);

#endif
