#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_rmdir.h"
#include "json_utils.h"

static struct arg_lit *rmdir_help_flag;
static struct arg_lit *rmdir_json_flag;
static struct arg_file *rmdir_dirs;
static struct arg_end *rmdir_end;
static void *rmdir_argtable[5];

static void build_rmdir_argtable(int max_dirs)
{
    rmdir_help_flag = arg_lit0("h", "help", "show help and exit");
    rmdir_json_flag = arg_lit0(NULL, "json", "output in JSON format");
    rmdir_dirs = arg_filen(NULL, NULL, "<DIR...>", 1, max_dirs, "empty directories to remove");
    rmdir_end = arg_end(20);

    rmdir_argtable[0] = rmdir_help_flag;
    rmdir_argtable[1] = rmdir_json_flag;
    rmdir_argtable[2] = rmdir_dirs;
    rmdir_argtable[3] = rmdir_end;
    rmdir_argtable[4] = NULL;
}

int rmdir_run(int argc, char **argv)
{
    int index;
    int status = 0;
    int nerrors;

    build_rmdir_argtable(argc + 2);

    nerrors = arg_parse(argc, argv, rmdir_argtable);

    if (rmdir_help_flag->count > 0) {
        rmdir_print_usage(stdout);
        arg_freetable(rmdir_argtable, 4);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, rmdir_end, "rmdir");
        rmdir_print_usage(stderr);
        arg_freetable(rmdir_argtable, 4);
        return 1;
    }

    if (rmdir_json_flag->count > 0) {
        printf("{\"command\":\"rmdir\",\"directories\":[");
    }
    for (index = 0; index < rmdir_dirs->count; index++) {
        if (rmdir(rmdir_dirs->filename[index]) != 0) {
            fprintf(stderr, "rmdir: %s: %s\n", rmdir_dirs->filename[index], strerror(errno));
            status = 1;
        } else if (rmdir_json_flag->count > 0) {
            if (index > 0) {
                putchar(',');
            }
            json_print_string(stdout, rmdir_dirs->filename[index]);
        }
    }
    if (rmdir_json_flag->count > 0) {
        printf("],\"success\":%s}\n", status == 0 ? "true" : "false");
    }

    arg_freetable(rmdir_argtable, 4);
    return status;
}

void rmdir_print_usage(FILE *out)
{
    build_rmdir_argtable(100);

    fprintf(out, "Usage: rmdir ");
    arg_print_syntax(out, rmdir_argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Remove one or more empty directories.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, rmdir_argtable, "  %-20s %s\n");

    arg_freetable(rmdir_argtable, 4);
}

cmd_spec_t cmd_rmdir_spec = {
    .name = "rmdir",
    .summary = "remove empty directories",
    .long_help = "Remove one or more empty directories.",
    .run = rmdir_run,
    .print_usage = rmdir_print_usage,
};

void register_rmdir_command(void)
{
    register_command(&cmd_rmdir_spec);
}
