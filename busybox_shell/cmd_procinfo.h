#ifndef CMD_PROCINFO_H
#define CMD_PROCINFO_H

#include <stdio.h>

int procinfo_run(int argc, char **argv);
void procinfo_print_usage(FILE *out);
void register_procinfo_command(void);

#endif
