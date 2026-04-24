#ifndef CMD_SPEC_H
#define CMD_SPEC_H

#include <stdio.h>

typedef struct cmd_spec {
    const char *name;
    const char *summary;
    const char *long_help;
    int (*run)(int argc, char **argv);
    void (*print_usage)(FILE *out);
} cmd_spec_t;

#endif
