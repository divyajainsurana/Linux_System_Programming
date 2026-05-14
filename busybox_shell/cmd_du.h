#ifndef CMD_DU_H
#define CMD_DU_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_du_spec;

int du_run(int argc, char **argv);
void du_print_usage(FILE *out);
void register_du_command(void);

#endif
