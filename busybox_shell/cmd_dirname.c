#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_dirname.h"
#include "json_utils.h"

static const char *dirname_value(const char *path, char *buffer, size_t buffer_size)
{
    size_t len;
    size_t slash_index;
    size_t dir_len;

    if (path == NULL || path[0] == '\0') {
        snprintf(buffer, buffer_size, ".");
        return buffer;
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
        snprintf(buffer, buffer_size, ".");
        return buffer;
    }

    dir_len = slash_index - 1;
    while (dir_len > 1 && path[dir_len - 1] == '/') {
        dir_len--;
    }

    if (dir_len == 0 && path[0] == '/') {
        snprintf(buffer, buffer_size, "/");
        return buffer;
    }

    snprintf(buffer, buffer_size, "%.*s", (int)dir_len, path);
    return buffer;
}

int dirname_run(int argc, char **argv)
{
    int index;
    int json = 0;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        dirname_print_usage(stdout);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json = 1;
    }

    if ((!json && argc < 2) || (json && argc < 3)) {
        fprintf(stderr, "dirname: missing operand\n");
        dirname_print_usage(stderr);
        return 1;
    }

    if (json) {
        printf("{\"paths\":[");
        for (index = 2; index < argc; index++) {
            char result[4096];

            if (index > 2) {
                putchar(',');
            }
            printf("{\"path\":");
            json_print_string(stdout, argv[index]);
            printf(",\"dirname\":");
            json_print_string(stdout, dirname_value(argv[index], result, sizeof(result)));
            printf("}");
        }
        printf("]}\n");
        return 0;
    }

    for (index = 1; index < argc; index++) {
        char result[4096];
        printf("%s\n", dirname_value(argv[index], result, sizeof(result)));
    }

    return 0;
}

void dirname_print_usage(FILE *out)
{
    fprintf(out, "Usage: dirname [--json] PATH...\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the directory portion of each path.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
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
