#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_cat.h"

static struct arg_lit *cat_help_flag;
static struct arg_file *cat_files;
static struct arg_end *cat_end;
static void *cat_argtable[4];

static void build_cat_argtable(int max_files)
{
    cat_help_flag = arg_lit0("h", "help", "show help and exit");
    cat_files = arg_filen(NULL, NULL, "[FILE...]", 0, max_files, "files to print");
    cat_end = arg_end(20);

    cat_argtable[0] = cat_help_flag;
    cat_argtable[1] = cat_files;
    cat_argtable[2] = cat_end;
    cat_argtable[3] = NULL;
}

static int print_stream(FILE *in, const char *name)
{
    char buffer[4096];
    size_t nread;

    while ((nread = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, nread, stdout) != nread) {
            fprintf(stderr, "cat: write error: %s\n", strerror(errno));
            return 1;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "cat: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return 0;
}

static int print_file(const char *path)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return print_stream(stdin, "standard input");
    }

    in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = print_stream(in, path);

    if (fclose(in) != 0) {
        fprintf(stderr, "cat: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

int cat_run(int argc, char **argv)
{
    int status = 0;

    build_cat_argtable(argc + 2);

    int nerrors = arg_parse(argc, argv, cat_argtable);

    if (cat_help_flag->count > 0) {
        cat_print_usage(stdout);
        arg_freetable(cat_argtable, 3);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, cat_end, "cat");
        cat_print_usage(stderr);
        arg_freetable(cat_argtable, 3);
        return 1;
    }

    if (cat_files->count == 0) {
        status = print_stream(stdin, "standard input");
    } else {
        int index;

        for (index = 0; index < cat_files->count; index++) {
            if (print_file(cat_files->filename[index]) != 0) {
                status = 1;
            }
        }
    }

    arg_freetable(cat_argtable, 3);
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

    arg_freetable(cat_argtable, 3);
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
