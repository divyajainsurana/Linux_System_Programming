#ifndef CMD_TOUCH_H
#define CMD_TOUCH_H

#include "cmd_spec.h"

extern cmd_spec_t cmd_touch_spec;

int touch_run(int argc, char **argv);
void touch_print_usage(FILE *out);
void register_touch_command(void);

#endif
