#include <stdio.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_pkg.h"

#define PKG_NAME "myshell"
#define PKG_VERSION "1.0.0"
#define PKG_DESCRIPTION "Modular CLI utilities in C"

static struct arg_lit *pkg_help_flag;
static struct arg_lit *pkg_json_flag;
static struct arg_end *pkg_end;
static void *pkg_argtable[4];

struct command_print_state {
    int first;
    int json;
};

static void build_pkg_argtable(void)
{
    pkg_help_flag = arg_lit0("h", "help", "show help and exit");
    pkg_json_flag = arg_lit0(NULL, "json", "output in JSON format");
    pkg_end = arg_end(20);

    pkg_argtable[0] = pkg_help_flag;
    pkg_argtable[1] = pkg_json_flag;
    pkg_argtable[2] = pkg_end;
    pkg_argtable[3] = NULL;
}

static void print_command(const cmd_spec_t *spec, void *userdata)
{
    struct command_print_state *state = userdata;

    if (state->json) {
        if (!state->first) {
            printf(",\n");
        }

        printf("    {\"name\":\"%s\",\"summary\":\"%s\"}",
               spec->name,
               spec->summary);
    } else {
        printf("  %s - %s\n", spec->name, spec->summary);
    }

    state->first = 0;
}

int pkg_run(int argc, char **argv)
{
    struct command_print_state state;

    build_pkg_argtable();

    int nerrors = arg_parse(argc, argv, pkg_argtable);

    if (pkg_help_flag->count > 0) {
        pkg_print_usage(stdout);
        arg_freetable(pkg_argtable, 3);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, pkg_end, "pkg");
        pkg_print_usage(stderr);
        arg_freetable(pkg_argtable, 3);
        return 1;
    }

    state.first = 1;
    state.json = pkg_json_flag->count > 0;

    if (state.json) {
        printf("{\n");
        printf("  \"name\":\"%s\",\n", PKG_NAME);
        printf("  \"version\":\"%s\",\n", PKG_VERSION);
        printf("  \"description\":\"%s\",\n", PKG_DESCRIPTION);
        printf("  \"commands\":[\n");
        for_each_command(print_command, &state);
        printf("\n  ]\n");
        printf("}\n");
    } else {
        printf("Package: %s\n", PKG_NAME);
        printf("Version: %s\n", PKG_VERSION);
        printf("Description: %s\n", PKG_DESCRIPTION);
        printf("Commands:\n");
        for_each_command(print_command, &state);
    }

    arg_freetable(pkg_argtable, 3);
    return 0;
}

void pkg_print_usage(FILE *out)
{
    build_pkg_argtable();

    fprintf(out, "Usage: pkg ");
    arg_print_syntax(out, pkg_argtable, "\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, pkg_argtable, "  %-20s %s\n");

    arg_freetable(pkg_argtable, 3);
}

cmd_spec_t cmd_pkg_spec = {
    .name = "pkg",
    .summary = "print package metadata",
    .long_help = "Print package metadata and registered command summaries.",
    .run = pkg_run,
    .print_usage = pkg_print_usage,
};

void register_pkg_command(void)
{
    register_command(&cmd_pkg_spec);
}
