#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_mv.h"
#include "json_utils.h"

int mv_run(int argc, char **argv)
{
    int json = 0;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        mv_print_usage(stdout);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json = 1;
        argv++;
        argc--;
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

    if (json) {
        printf("{\"command\":\"mv\",\"source\":");
        json_print_string(stdout, argv[1]);
        printf(",\"destination\":");
        json_print_string(stdout, argv[2]);
        printf(",\"moved\":true}\n");
    }

    return 0;
}

void mv_print_usage(FILE *out)
{
    fprintf(out, "Usage: mv [--json] SOURCE DEST\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Rename or move SOURCE to DEST.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
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
