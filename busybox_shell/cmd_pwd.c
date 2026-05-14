#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_pwd.h"

static char *get_current_directory(void)
{
    size_t size = 128;
    char *buffer = NULL;

    while (1) {
        char *resized = realloc(buffer, size);
        if (resized == NULL) {
            free(buffer);
            return NULL;
        }

        buffer = resized;

        if (getcwd(buffer, size) != NULL) {
            return buffer;
        }

        if (errno != ERANGE) {
            free(buffer);
            return NULL;
        }

        size *= 2;
    }
}

int pwd_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *argtable[3];
    char *cwd;
    int nerrors;

    help = arg_lit0("h", "help", "show help and exit");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = end;
    argtable[2] = NULL;

    nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        pwd_print_usage(stdout);
        arg_freetable(argtable, 2);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "pwd");
        pwd_print_usage(stderr);
        arg_freetable(argtable, 2);
        return 1;
    }

    cwd = get_current_directory();
    if (cwd == NULL) {
        fprintf(stderr, "pwd: %s\n", strerror(errno));
        arg_freetable(argtable, 2);
        return 1;
    }

    printf("%s\n", cwd);

    free(cwd);
    arg_freetable(argtable, 2);
    return 0;
}

void pwd_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *argtable[3];

    help = arg_lit0("h", "help", "show help and exit");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = end;
    argtable[2] = NULL;

    fprintf(out, "Usage: pwd ");
    arg_print_syntax(out, argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the absolute path of the current working directory.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-20s %s\n");

    arg_freetable(argtable, 2);
}

cmd_spec_t cmd_pwd_spec = {
    .name = "pwd",
    .summary = "print the current working directory",
    .long_help = "Print the absolute path of the current working directory.",
    .run = pwd_run,
    .print_usage = pwd_print_usage,
};

void register_pwd_command(void)
{
    register_command(&cmd_pwd_spec);
}
