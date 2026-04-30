#include <stdio.h>
#include <time.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_localdate.h"

int localdate_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *argtable[3];

    help = arg_lit0("h", "help", "show help and exit");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = end;
    argtable[2] = NULL;

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        localdate_print_usage(stdout);
        arg_freetable(argtable, 2);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "localdate");
        localdate_print_usage(stderr);
        arg_freetable(argtable, 2);
        return 1;
    }

    time_t now = time(NULL);
    struct tm *local = localtime(&now);

    if (local == NULL) {
        fprintf(stderr, "localdate: failed to get local time\n");
        arg_freetable(argtable, 2);
        return 1;
    }

    printf("Local Date: %04d-%02d-%02d\n",
           local->tm_year + 1900,
           local->tm_mon + 1,
           local->tm_mday);

    arg_freetable(argtable, 2);
    return 0;
}

void localdate_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *argtable[3];

    help = arg_lit0("h", "help", "show help and exit");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = end;
    argtable[2] = NULL;

    fprintf(out, "Usage: localdate ");
    arg_print_syntax(out, argtable, "\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-20s %s\n");

    arg_freetable(argtable, 2);
}

cmd_spec_t cmd_localdate_spec = {
    .name = "localdate",
    .summary = "print the current local date",
    .long_help = "Print the current local date in YYYY-MM-DD format.",
    .run = localdate_run,
    .print_usage = localdate_print_usage,
};

void register_localdate_command(void)
{
    register_command(&cmd_localdate_spec);
}