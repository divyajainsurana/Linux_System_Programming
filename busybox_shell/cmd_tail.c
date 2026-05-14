#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_tail.h"
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

static int append_char(char **buffer, size_t *length, size_t *capacity, int ch)
{
    char *resized;

    if (*length == *capacity) {
        size_t next_capacity = *capacity == 0 ? 4096 : *capacity * 2;
        resized = realloc(*buffer, next_capacity);
        if (resized == NULL) {
            return 1;
        }
        *buffer = resized;
        *capacity = next_capacity;
    }

    (*buffer)[(*length)++] = (char)ch;
    return 0;
}

static int print_tail_stream(FILE *in, const char *name, long count)
{
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t start;
    size_t pos;
    long lines = 0;
    int ch;
    int status = 0;

    while ((ch = fgetc(in)) != EOF) {
        if (append_char(&buffer, &length, &capacity, ch) != 0) {
            fprintf(stderr, "tail: out of memory\n");
            free(buffer);
            return 1;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "tail: %s: %s\n", name, strerror(errno));
        free(buffer);
        return 1;
    }

    if (count == 0 || length == 0) {
        free(buffer);
        return 0;
    }

    pos = length;
    if (pos > 0 && buffer[pos - 1] == '\n') {
        pos--;
    }

    while (pos > 0 && lines < count) {
        pos--;
        if (buffer[pos] == '\n') {
            lines++;
        }
    }

    start = lines == count ? pos + 1 : 0;
    if (fwrite(buffer + start, 1, length - start, stdout) != length - start) {
        fprintf(stderr, "tail: write error: %s\n", strerror(errno));
        status = 1;
    }

    free(buffer);
    return status;
}

static int print_tail_bytes_stream(FILE *in, const char *name, long count)
{
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t start;
    int ch;
    int status = 0;

    while ((ch = fgetc(in)) != EOF) {
        if (append_char(&buffer, &length, &capacity, ch) != 0) {
            fprintf(stderr, "tail: out of memory\n");
            free(buffer);
            return 1;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "tail: %s: %s\n", name, strerror(errno));
        free(buffer);
        return 1;
    }

    start = (count >= (long)length) ? 0 : length - (size_t)count;
    if (fwrite(buffer + start, 1, length - start, stdout) != length - start) {
        fprintf(stderr, "tail: write error: %s\n", strerror(errno));
        status = 1;
    }

    free(buffer);
    return status;
}

static int print_tail_stream_json(FILE *in, const char *name, long count)
{
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t start;
    size_t pos;
    long lines = 0;
    int ch;
    int status = 0;

    while ((ch = fgetc(in)) != EOF) {
        if (append_char(&buffer, &length, &capacity, ch) != 0) {
            fprintf(stderr, "tail: out of memory\n");
            free(buffer);
            return 1;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "tail: %s: %s\n", name, strerror(errno));
        free(buffer);
        return 1;
    }

    printf("{\"path\":");
    json_print_string(stdout, name);
    printf(",\"content\":\"");

    if (count != 0 && length != 0) {
        pos = length;
        if (pos > 0 && buffer[pos - 1] == '\n') {
            pos--;
        }

        while (pos > 0 && lines < count) {
            pos--;
            if (buffer[pos] == '\n') {
                lines++;
            }
        }

        start = lines == count ? pos + 1 : 0;
        for (pos = start; pos < length; pos++) {
            json_print_escaped_char(stdout, buffer[pos]);
        }
    }

    printf("\"}");
    if (ferror(stdout)) {
        status = 1;
    }

    free(buffer);
    return status;
}

static int print_tail_bytes_stream_json(FILE *in, const char *name, long count)
{
    char *buffer = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t start;
    size_t pos;
    int ch;
    int status = 0;

    while ((ch = fgetc(in)) != EOF) {
        if (append_char(&buffer, &length, &capacity, ch) != 0) {
            fprintf(stderr, "tail: out of memory\n");
            free(buffer);
            return 1;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "tail: %s: %s\n", name, strerror(errno));
        free(buffer);
        return 1;
    }

    printf("{\"path\":");
    json_print_string(stdout, name);
    printf(",\"content\":\"");
    start = (count >= (long)length) ? 0 : length - (size_t)count;
    for (pos = start; pos < length; pos++) {
        json_print_escaped_char(stdout, buffer[pos]);
    }
    printf("\"}");

    if (ferror(stdout)) {
        status = 1;
    }

    free(buffer);
    return status;
}

static int print_tail_file_json(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_tail_stream_json(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_tail_stream_json(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int print_tail_bytes_file_json(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_tail_bytes_stream_json(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_tail_bytes_stream_json(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int print_tail_file(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_tail_stream(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_tail_stream(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int print_tail_bytes_file(const char *path, long count)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_tail_bytes_stream(stdin, "standard input", count);
    }

    in = fopen(path, "r");
    if (in == NULL) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_tail_bytes_stream(in, path, count);
    if (fclose(in) != 0) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int follow_file(const char *path)
{
    FILE *in = fopen(path, "r");
    int ch;

    if (in == NULL) {
        fprintf(stderr, "tail: %s: %s\n", path, strerror(errno));
        return 1;
    }

    fseek(in, 0, SEEK_END);
    while (1) {
        ch = fgetc(in);
        if (ch == EOF) {
            clearerr(in);
            sleep(1);
            continue;
        }
        putchar(ch);
        fflush(stdout);
    }
}

int tail_run(int argc, char **argv)
{
    long count = 10;
    int index = 1;
    int status = 0;
    int json = 0;
    int bytes_mode = 0;
    int quiet = 0;
    int verbose = 0;
    int follow = 0;

    while (index < argc) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            tail_print_usage(stdout);
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
        if (strcmp(argv[index], "-f") == 0) {
            follow = 1;
            index++;
            continue;
        }
        if (strcmp(argv[index], "-n") == 0) {
            if (index + 1 >= argc || parse_count(argv[index + 1], &count) != 0) {
                fprintf(stderr, "tail: invalid line count\n");
                tail_print_usage(stderr);
                return 1;
            }
            index += 2;
            continue;
        }
        if (strcmp(argv[index], "-c") == 0) {
            if (index + 1 >= argc || parse_count(argv[index + 1], &count) != 0) {
                fprintf(stderr, "tail: invalid byte count\n");
                tail_print_usage(stderr);
                return 1;
            }
            bytes_mode = 1;
            index += 2;
            continue;
        }
        break;
    }

    if (follow) {
        if (json) {
            fprintf(stderr, "tail: -f is not supported with --json\n");
            return 1;
        }
        if (index >= argc) {
            fprintf(stderr, "tail: -f requires a file\n");
            return 1;
        }
        return follow_file(argv[index]);
    }

    if (json) {
        int file_index;

        printf("{\"command\":\"tail\",\"%s\":%ld,\"files\":[",
               bytes_mode ? "bytes" : "lines", count);
        if (index == argc) {
            status = bytes_mode ?
                print_tail_bytes_stream_json(stdin, "standard input", count) :
                print_tail_stream_json(stdin, "standard input", count);
        } else {
            for (file_index = index; file_index < argc; file_index++) {
                if (file_index > index) {
                    putchar(',');
                }
                if ((bytes_mode ? print_tail_bytes_file_json(argv[file_index], count) :
                                  print_tail_file_json(argv[file_index], count)) != 0) {
                    status = 1;
                }
            }
        }
        printf("]}\n");
        return status;
    }

    if (index == argc) {
        return bytes_mode ?
            print_tail_bytes_stream(stdin, "standard input", count) :
            print_tail_stream(stdin, "standard input", count);
    }

    for (; index < argc; index++) {
        if ((verbose || (!quiet && argc - index > 1)) && argc > 2) {
            printf("==> %s <==\n", argv[index]);
        }
        if ((bytes_mode ? print_tail_bytes_file(argv[index], count) :
                          print_tail_file(argv[index], count)) != 0) {
            status = 1;
        }
    }

    return status;
}

void tail_print_usage(FILE *out)
{
    fprintf(out, "Usage: tail [--json] [-n LINES|-c BYTES] [-q|-v] [-f] [FILE...]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the last lines of files or standard input.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
    fprintf(out, "  %-20s %s\n", "-n LINES", "number of lines to print (default: 10)");
    fprintf(out, "  %-20s %s\n", "-c BYTES", "number of bytes to print");
    fprintf(out, "  %-20s %s\n", "-q", "never print file headers");
    fprintf(out, "  %-20s %s\n", "-v", "always print file headers");
    fprintf(out, "  %-20s %s\n", "-f", "follow appended data");
}

cmd_spec_t cmd_tail_spec = {
    .name = "tail",
    .summary = "print the last lines of files",
    .long_help = "Print the last lines of files or standard input.",
    .run = tail_run,
    .print_usage = tail_print_usage,
};

void register_tail_command(void)
{
    register_command(&cmd_tail_spec);
}
