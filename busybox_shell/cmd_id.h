#ifndef CMD_ID_H
#define CMD_ID_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_id_spec;

int id_run(int argc, char **argv);
void id_print_usage(FILE *out);
void register_id_command(void);

#endif
