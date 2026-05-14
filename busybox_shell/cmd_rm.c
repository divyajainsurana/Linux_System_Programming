#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_rm.h"
#include "json_utils.h"

int rm_run(int argc, char **argv)
{
    int force = 0;
    int index = 1;
    int status = 0;
    int json = 0;
    int printed = 0;

    while (index < argc) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            rm_print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[index], "-f") == 0 || strcmp(argv[index], "--force") == 0) {
            force = 1;
            index++;
            continue;
        }
        if (strcmp(argv[index], "--json") == 0) {
            json = 1;
            index++;
            continue;
        }
        break;
    }

    if (index == argc) {
        if (!force) {
            fprintf(stderr, "rm: missing file operand\n");
            rm_print_usage(stderr);
            return 1;
        }
        if (json) {
            printf("{\"command\":\"rm\",\"removed\":[]}\n");
        }
        return 0;
    }

    if (json) {
        printf("{\"command\":\"rm\",\"removed\":[");
    }
    for (; index < argc; index++) {
        if (unlink(argv[index]) != 0) {
            if (force && errno == ENOENT) {
                continue;
            }
            fprintf(stderr, "rm: %s: %s\n", argv[index], strerror(errno));
            status = 1;
        } else if (json) {
            if (printed) {
                putchar(',');
            }
            json_print_string(stdout, argv[index]);
            printed = 1;
        }
    }
    if (json) {
        printf("],\"success\":%s}\n", status == 0 ? "true" : "false");
    }

    return status;
}

void rm_print_usage(FILE *out)
{
    fprintf(out, "Usage: rm [--json] [-f] FILE...\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Remove files.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
    fprintf(out, "  %-20s %s\n", "-f, --force", "ignore missing files");
}

cmd_spec_t cmd_rm_spec = {
    .name = "rm",
    .summary = "remove files",
    .long_help = "Remove files.",
    .run = rm_run,
    .print_usage = rm_print_usage,
};

void register_rm_command(void)
{
    register_command(&cmd_rm_spec);
}
