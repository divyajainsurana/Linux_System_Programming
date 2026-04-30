#ifndef CMD_PKG_H
#define CMD_PKG_H

#include <stdio.h>
#include "cmd_spec.h"

int pkg_run(int argc, char **argv);
void pkg_print_usage(FILE *out);

extern cmd_spec_t cmd_pkg_spec;

void register_pkg_command(void);

#endif
