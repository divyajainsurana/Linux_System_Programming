#ifndef CMD_PWD_H
#define CMD_PWD_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_pwd_spec;

int pwd_run(int argc, char **argv);
void pwd_print_usage(FILE *out);
void register_pwd_command(void);

#endif
