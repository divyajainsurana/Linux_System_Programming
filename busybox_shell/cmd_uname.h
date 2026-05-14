#ifndef CMD_UNAME_H
#define CMD_UNAME_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_uname_spec;

int uname_run(int argc, char **argv);
void uname_print_usage(FILE *out);
void register_uname_command(void);

#endif
