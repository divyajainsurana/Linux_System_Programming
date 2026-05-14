#ifndef CMD_WC_H
#define CMD_WC_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_wc_spec;

int wc_run(int argc, char **argv);
void wc_print_usage(FILE *out);
void register_wc_command(void);

#endif
