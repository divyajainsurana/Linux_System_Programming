#ifndef CMD_CAT_H
#define CMD_CAT_H

#include <stdio.h>
#include "cmd_spec.h"

int cat_run(int argc, char **argv);
void cat_print_usage(FILE *out);

extern cmd_spec_t cmd_cat_spec;

void register_cat_command(void);

#endif
