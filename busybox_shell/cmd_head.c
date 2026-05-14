#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_head.h"
#include "json_utils.h"

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

static int print_head_bytes_stream(FILE *in, const char *name, long count)
{
    int ch;
    long bytes = 0;

    while (bytes < count && (ch = fgetc(in)) != EOF) {
        putchar(ch);
        bytes++;
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

static int print_head_bytes_file(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_head_bytes_stream(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_head_bytes_stream(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int print_head_stream_json(FILE *in, const char *name, long count)
{
    int ch;
    long lines = 0;

    printf("{\"path\":");
    json_print_string(stdout, name);
    printf(",\"content\":\"");
    while (lines < count && (ch = fgetc(in)) != EOF) {
        json_print_escaped_char(stdout, ch);
        if (ch == '\n') {
            lines++;
        }
    }
    printf("\"}");

    if (ferror(in)) {
        fprintf(stderr, "head: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return ferror(stdout) ? 1 : 0;
}

static int print_head_bytes_stream_json(FILE *in, const char *name, long count)
{
    int ch;
    long bytes = 0;

    printf("{\"path\":");
    json_print_string(stdout, name);
    printf(",\"content\":\"");
    while (bytes < count && (ch = fgetc(in)) != EOF) {
        json_print_escaped_char(stdout, ch);
        bytes++;
    }
    printf("\"}");

    if (ferror(in)) {
        fprintf(stderr, "head: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return ferror(stdout) ? 1 : 0;
}

static int print_head_file_json(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_head_stream_json(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_head_stream_json(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int print_head_bytes_file_json(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_head_bytes_stream_json(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "head: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_head_bytes_stream_json(in, path, count);
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
    int json = 0;
    int bytes_mode = 0;
    int quiet = 0;
    int verbose = 0;

    while (index < argc) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            head_print_usage(stdout);
            return 0;
        }
        if (strcmp(argv[index], "--json") == 0) {
            json = 1;
            index++;
            continue;
        }
        if (strcmp(argv[index], "-q") == 0) {
            quiet = 1;
            verbose = 0;
            index++;
            continue;
        }
        if (strcmp(argv[index], "-v") == 0) {
            verbose = 1;
            quiet = 0;
            index++;
            continue;
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
        if (strcmp(argv[index], "-c") == 0) {
            if (index + 1 >= argc || parse_count(argv[index + 1], &count) != 0) {
                fprintf(stderr, "head: invalid byte count\n");
                head_print_usage(stderr);
                return 1;
            }
            bytes_mode = 1;
            index += 2;
            continue;
        }
        break;
    }

    if (json) {
        int file_index;

        printf("{\"command\":\"head\",\"%s\":%ld,\"files\":[",
               bytes_mode ? "bytes" : "lines", count);
        if (index == argc) {
            status = bytes_mode ?
                print_head_bytes_stream_json(stdin, "standard input", count) :
                print_head_stream_json(stdin, "standard input", count);
        } else {
            for (file_index = index; file_index < argc; file_index++) {
                if (file_index > index) {
                    putchar(',');
                }
                if ((bytes_mode ? print_head_bytes_file_json(argv[file_index], count) :
                                  print_head_file_json(argv[file_index], count)) != 0) {
                    status = 1;
                }
            }
        }
        printf("]}\n");
        return status;
    }

    if (index == argc) {
        return bytes_mode ?
            print_head_bytes_stream(stdin, "standard input", count) :
            print_head_stream(stdin, "standard input", count);
    }

    for (; index < argc; index++) {
        if ((verbose || (!quiet && argc - index > 1)) && argc > 2) {
            printf("==> %s <==\n", argv[index]);
        }
        if ((bytes_mode ? print_head_bytes_file(argv[index], count) :
                          print_head_file(argv[index], count)) != 0) {
            status = 1;
        }
    }

    return status;
}

void head_print_usage(FILE *out)
{
    fprintf(out, "Usage: head [--json] [-n LINES|-c BYTES] [-q|-v] [FILE...]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the first lines of files or standard input.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
    fprintf(out, "  %-20s %s\n", "-n LINES", "number of lines to print (default: 10)");
    fprintf(out, "  %-20s %s\n", "-c BYTES", "number of bytes to print");
    fprintf(out, "  %-20s %s\n", "-q", "never print file headers");
    fprintf(out, "  %-20s %s\n", "-v", "always print file headers");
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
