#ifndef CMD_MV_H
#define CMD_MV_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_mv_spec;

int mv_run(int argc, char **argv);
void mv_print_usage(FILE *out);
void register_mv_command(void);

#endif
