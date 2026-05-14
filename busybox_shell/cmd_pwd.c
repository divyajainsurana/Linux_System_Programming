#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_pwd.h"
#include "json_utils.h"

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
    struct arg_lit *json;
    struct arg_lit *logical_flag;
    struct arg_lit *physical_flag;
    struct arg_end *end;
    void *argtable[6];
    char *cwd;
    int nerrors;
    int logical = 0;

    help = arg_lit0("h", "help", "show help and exit");
    json = arg_lit0(NULL, "json", "output in JSON format");
    logical_flag = arg_lit0("L", NULL, "print logical path from PWD when available");
    physical_flag = arg_lit0("P", NULL, "print physical current directory");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = json;
    argtable[2] = logical_flag;
    argtable[3] = physical_flag;
    argtable[4] = end;
    argtable[5] = NULL;

    nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        pwd_print_usage(stdout);
        arg_freetable(argtable, 5);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "pwd");
        pwd_print_usage(stderr);
        arg_freetable(argtable, 5);
        return 1;
    }

    if (logical_flag->count > 0 && physical_flag->count == 0 && getenv("PWD") != NULL) {
        const char *pwd_env = getenv("PWD");
        cwd = malloc(strlen(pwd_env) + 1);
        if (cwd != NULL) {
            strcpy(cwd, pwd_env);
        }
        logical = 1;
    } else {
        cwd = get_current_directory();
    }
    if (cwd == NULL) {
        fprintf(stderr, "pwd: %s\n", strerror(errno));
        arg_freetable(argtable, 5);
        return 1;
    }

    if (json->count > 0) {
        printf("{\"cwd\":");
        json_print_string(stdout, cwd);
        printf(",\"logical\":%s}\n", logical ? "true" : "false");
    } else {
        printf("%s\n", cwd);
    }

    free(cwd);
    arg_freetable(argtable, 5);
    return 0;
}

void pwd_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *json;
    struct arg_lit *logical_flag;
    struct arg_lit *physical_flag;
    struct arg_end *end;
    void *argtable[6];

    help = arg_lit0("h", "help", "show help and exit");
    json = arg_lit0(NULL, "json", "output in JSON format");
    logical_flag = arg_lit0("L", NULL, "print logical path from PWD when available");
    physical_flag = arg_lit0("P", NULL, "print physical current directory");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = json;
    argtable[2] = logical_flag;
    argtable[3] = physical_flag;
    argtable[4] = end;
    argtable[5] = NULL;

    fprintf(out, "Usage: pwd ");
    arg_print_syntax(out, argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the absolute path of the current working directory.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-20s %s\n");
    fprintf(out, "  %-20s %s\n", "-L", "print logical path from PWD when available");
    fprintf(out, "  %-20s %s\n", "-P", "print physical current directory");

    arg_freetable(argtable, 5);
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
