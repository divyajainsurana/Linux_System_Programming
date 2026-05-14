#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_dirname.h"

static void print_dirname(const char *path)
{
    size_t len;
    size_t slash_index;
    size_t dir_len;

    if (path == NULL || path[0] == '\0') {
        printf(".\n");
        return;
    }

    len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        len--;
    }

    slash_index = len;
    while (slash_index > 0 && path[slash_index - 1] != '/') {
        slash_index--;
    }

    if (slash_index == 0) {
        printf(".\n");
        return;
    }

    dir_len = slash_index - 1;
    while (dir_len > 1 && path[dir_len - 1] == '/') {
        dir_len--;
    }

    if (dir_len == 0 && path[0] == '/') {
        printf("/\n");
        return;
    }

    printf("%.*s\n", (int)dir_len, path);
}

int dirname_run(int argc, char **argv)
{
    int index;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        dirname_print_usage(stdout);
        return 0;
    }

    if (argc < 2) {
        fprintf(stderr, "dirname: missing operand\n");
        dirname_print_usage(stderr);
        return 1;
    }

    for (index = 1; index < argc; index++) {
        print_dirname(argv[index]);
    }

    return 0;
}

void dirname_print_usage(FILE *out)
{
    fprintf(out, "Usage: dirname PATH...\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the directory portion of each path.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
}

cmd_spec_t cmd_dirname_spec = {
    .name = "dirname",
    .summary = "print directory portion of paths",
    .long_help = "Print the directory portion of each path.",
    .run = dirname_run,
    .print_usage = dirname_print_usage,
};

void register_dirname_command(void)
{
    register_command(&cmd_dirname_spec);
}
