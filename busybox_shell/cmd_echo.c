#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_echo.h"

int echo_run(int argc, char **argv)
{
    int index = 1;
    int trailing_newline = 1;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        echo_print_usage(stdout);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "-n") == 0) {
        trailing_newline = 0;
        index = 2;
    }

    for (; index < argc; index++) {
        if (index > (trailing_newline ? 1 : 2)) {
            putchar(' ');
        }
        fputs(argv[index], stdout);
    }

    if (trailing_newline) {
        putchar('\n');
    }

    return ferror(stdout) ? 1 : 0;
}

void echo_print_usage(FILE *out)
{
    fprintf(out, "Usage: echo [-n] [TEXT...]\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print text arguments separated by spaces.\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-n", "do not print the trailing newline");
}

cmd_spec_t cmd_echo_spec = {
    .name = "echo",
    .summary = "print text to standard output",
    .long_help = "Print text arguments separated by spaces.",
    .run = echo_run,
    .print_usage = echo_print_usage,
};

void register_echo_command(void)
{
    register_command(&cmd_echo_spec);
}
