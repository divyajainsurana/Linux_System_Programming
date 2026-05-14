#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_mv.h"

int mv_run(int argc, char **argv)
{
    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        mv_print_usage(stdout);
        return 0;
    }

    if (argc != 3) {
        fprintf(stderr, "mv: expected SOURCE and DEST\n");
        mv_print_usage(stderr);
        return 1;
    }

    if (rename(argv[1], argv[2]) != 0) {
        fprintf(stderr, "mv: %s to %s: %s\n", argv[1], argv[2], strerror(errno));
        return 1;
    }

    return 0;
}

void mv_print_usage(FILE *out)
{
    fprintf(out, "Usage: mv SOURCE DEST\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Rename or move SOURCE to DEST.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
}

cmd_spec_t cmd_mv_spec = {
    .name = "mv",
    .summary = "move or rename files",
    .long_help = "Rename or move SOURCE to DEST.",
    .run = mv_run,
    .print_usage = mv_print_usage,
};

void register_mv_command(void)
{
    register_command(&cmd_mv_spec);
}
