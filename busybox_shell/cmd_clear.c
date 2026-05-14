#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_clear.h"

int clear_run(int argc, char **argv)
{
    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        clear_print_usage(stdout);
        return 0;
    }

    if (argc > 1) {
        fprintf(stderr, "clear: too many arguments\n");
        clear_print_usage(stderr);
        return 1;
    }

    fputs("\033[H\033[J", stdout);
    return ferror(stdout) ? 1 : 0;
}

void clear_print_usage(FILE *out)
{
    fprintf(out, "Usage: clear [-h]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Clear the terminal screen.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
}

cmd_spec_t cmd_clear_spec = {
    .name = "clear",
    .summary = "clear the terminal screen",
    .long_help = "Clear the terminal screen.",
    .run = clear_run,
    .print_usage = clear_print_usage,
};

void register_clear_command(void)
{
    register_command(&cmd_clear_spec);
}
