#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_head.h"

static int parse_count(const char *text, long *count)
{
    char *endptr;
    long value;

    errno = 0;
    value = strtol(text, &endptr, 10);
    if (errno != 0 || *endptr != '\0' || value < 0) {
        return 1;
    }

    *count = value;
    return 0;
}

static int print_head_stream(FILE *in, const char *name, long count)
{
    int ch;
    long lines = 0;

    while (lines < count && (ch = fgetc(in)) != EOF) {
        putchar(ch);
        if (ch == '\n') {
            lines++;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "head: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return ferror(stdout) ? 1 : 0;
}

static int print_head_file(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_head_stream(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_head_stream(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

int head_run(int argc, char **argv)
{
    long count = 10;
    int index = 1;
    int status = 0;

    while (index < argc) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            head_print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[index], "-n") == 0) {
            if (index + 1 >= argc || parse_count(argv[index + 1], &count) != 0) {
                fprintf(stderr, "head: invalid line count\n");
                head_print_usage(stderr);
                return 1;
            }
            index += 2;
            continue;
        }
        break;
    }

    if (index == argc) {
        return print_head_stream(stdin, "standard input", count);
    }

    for (; index < argc; index++) {
        if (print_head_file(argv[index], count) != 0) {
            status = 1;
        }
    }

    return status;
}

void head_print_usage(FILE *out)
{
    fprintf(out, "Usage: head [-n LINES] [FILE...]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the first lines of files or standard input.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-n LINES", "number of lines to print (default: 10)");
}

cmd_spec_t cmd_head_spec = {
    .name = "head",
    .summary = "print the first lines of files",
    .long_help = "Print the first lines of files or standard input.",
    .run = head_run,
    .print_usage = head_print_usage,
};

void register_head_command(void)
{
    register_command(&cmd_head_spec);
}
