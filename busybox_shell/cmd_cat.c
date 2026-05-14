#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_cat.h"
#include "json_utils.h"

struct cat_options {
    int number_all;
    int number_nonblank;
    int squeeze_blank;
    int show_ends;
};

static struct arg_lit *cat_help_flag;
static struct arg_lit *cat_number_all_flag;
static struct arg_lit *cat_number_nonblank_flag;
static struct arg_lit *cat_squeeze_blank_flag;
static struct arg_lit *cat_show_ends_flag;
static struct arg_file *cat_files;
static struct arg_end *cat_end;
static void *cat_argtable[8];

static void build_cat_argtable(int max_files)
{
    cat_help_flag = arg_lit0("h", "help", "show help and exit");
    cat_number_all_flag = arg_lit0("n", "number", "number all output lines");
    cat_number_nonblank_flag = arg_lit0("b", "number-nonblank", "number non-empty output lines");
    cat_squeeze_blank_flag = arg_lit0("s", "squeeze-blank", "suppress repeated empty output lines");
    cat_show_ends_flag = arg_lit0("E", "show-ends", "display $ at end of each line");
    cat_files = arg_filen(NULL, NULL, "[FILE...]", 0, max_files, "files to print");
    cat_end = arg_end(20);

    cat_argtable[0] = cat_help_flag;
    cat_argtable[1] = cat_number_all_flag;
    cat_argtable[2] = cat_number_nonblank_flag;
    cat_argtable[3] = cat_squeeze_blank_flag;
    cat_argtable[4] = cat_show_ends_flag;
    cat_argtable[5] = cat_files;
    cat_argtable[6] = cat_end;
    cat_argtable[7] = NULL;
}

static int print_stream(FILE *in, const char *name, const struct cat_options *opts)
{
    int ch;
    int at_line_start = 1;
    int blank_run = 0;
    unsigned long line_number = 1;

    while ((ch = fgetc(in)) != EOF) {
        if (at_line_start) {
            if (ch == '\n') {
                blank_run++;
                if (opts->squeeze_blank && blank_run > 1) {
                    continue;
                }
            } else {
                blank_run = 0;
            }

            if (opts->number_all || (opts->number_nonblank && ch != '\n')) {
                printf("%6lu\t", line_number++);
            }
            at_line_start = 0;
        }

        if (ch == '\n') {
            if (opts->show_ends) {
                putchar('$');
            }
            putchar('\n');
            at_line_start = 1;
        } else {
            putchar(ch);
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return 0;
}

static int print_file(const char *path, const struct cat_options *opts)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_stream(stdin, "standard input", opts);
    }

    in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_stream(in, path, opts);

    if (fclose(in) != 0) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static int print_stream_json(FILE *in, const char *name)
{
    int ch;

    printf("{\"path\":");
    json_print_string(stdout, name);
    printf(",\"content\":\"");
    while ((ch = fgetc(in)) != EOF) {
        json_print_escaped_char(stdout, ch);
    }
    printf("\"}");

    if (ferror(in)) {
        fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return ferror(stdout) ? 1 : 0;
}

static int print_file_json(const char *path)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_stream_json(stdin, "standard input");
    }

    in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_stream_json(in, path);

    if (fclose(in) != 0) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

int cat_run(int argc, char **argv)
{
    int status = 0;
    int json = 0;
    int json_start = 1;
    struct cat_options opts = {0, 0, 0, 0};

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json = 1;
        json_start = 2;
    }

    if (json) {
        int index;

        printf("{\"command\":\"cat\",\"files\":[");
        if (json_start == argc) {
            status = print_stream_json(stdin, "standard input");
        } else {
            for (index = json_start; index < argc; index++) {
                if (index > json_start) {
                    putchar(',');
                }
                if (print_file_json(argv[index]) != 0) {
                    status = 1;
                }
            }
        }
        printf("]}\n");
        return status;
    }

    build_cat_argtable(argc + 2);

    int nerrors = arg_parse(argc, argv, cat_argtable);

    if (cat_help_flag->count > 0) {
        cat_print_usage(stdout);
        arg_freetable(cat_argtable, 7);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, cat_end, "cat");
        cat_print_usage(stderr);
        arg_freetable(cat_argtable, 7);
        return 1;
    }

    opts.number_all = cat_number_all_flag->count > 0;
    opts.number_nonblank = cat_number_nonblank_flag->count > 0;
    opts.squeeze_blank = cat_squeeze_blank_flag->count > 0;
    opts.show_ends = cat_show_ends_flag->count > 0;
    if (opts.number_nonblank) {
        opts.number_all = 0;
    }

    if (cat_files->count == 0) {
        status = print_stream(stdin, "standard input", &opts);
    } else {
        int index;

        for (index = 0; index < cat_files->count; index++) {
            if (print_file(cat_files->filename[index], &opts) != 0) {
                status = 1;
            }
        }
    }

    arg_freetable(cat_argtable, 7);
    return status;
}

void cat_print_usage(FILE *out)
{
    build_cat_argtable(100);

    fprintf(out, "Usage: cat ");
    arg_print_syntax(out, cat_argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print file contents to standard output.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, cat_argtable, "  %-20s %s\n");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");

    arg_freetable(cat_argtable, 7);
}

cmd_spec_t cmd_cat_spec = {
    .name = "cat",
    .summary = "concatenate and print files",
    .long_help = "Print file contents to standard output.",
    .run = cat_run,
    .print_usage = cat_print_usage,
};

void register_cat_command(void)
{
    register_command(&cmd_cat_spec);
}
