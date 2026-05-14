#ifndef CMD_WHOAMI_H
#define CMD_WHOAMI_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_whoami_spec;

int whoami_run(int argc, char **argv);
void whoami_print_usage(FILE *out);
void register_whoami_command(void);

#endif
