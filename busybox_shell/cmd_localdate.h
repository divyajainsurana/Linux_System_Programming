#ifndef CMD_LOCALDATE_H
#define CMD_LOCALDATE_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_localdate_spec;

int localdate_run(int argc, char **argv);
void localdate_print_usage(FILE *out);
void register_localdate_command(void);

#endif
