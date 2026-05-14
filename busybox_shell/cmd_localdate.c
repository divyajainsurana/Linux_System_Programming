#include <stdio.h>
#include <time.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_localdate.h"

int localdate_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_lit *json;
    struct arg_end *end;
    void *argtable[4];

    help = arg_lit0("h", "help", "show help and exit");
    json = arg_lit0(NULL, "json", "output in JSON format");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = json;
    argtable[2] = end;
    argtable[3] = NULL;

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        localdate_print_usage(stdout);
        arg_freetable(argtable, 3);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "localdate");
        localdate_print_usage(stderr);
        arg_freetable(argtable, 3);
        return 1;
    }

    time_t now = time(NULL);
    struct tm *local = localtime(&now);

    if (local == NULL) {
        fprintf(stderr, "localdate: failed to get local time\n");
        arg_freetable(argtable, 3);
        return 1;
    }

    if (json->count > 0) {
        printf("{\"date\":\"%04d-%02d-%02d\",\"year\":%d,\"month\":%d,\"day\":%d}\n",
               local->tm_year + 1900,
               local->tm_mon + 1,
               local->tm_mday,
               local->tm_year + 1900,
               local->tm_mon + 1,
               local->tm_mday);
    } else {
        printf("Local Date: %04d-%02d-%02d\n",
               local->tm_year + 1900,
               local->tm_mon + 1,
               local->tm_mday);
    }

    arg_freetable(argtable, 3);
    return 0;
}

void localdate_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_lit *json;
    struct arg_end *end;
    void *argtable[4];

    help = arg_lit0("h", "help", "show help and exit");
    json = arg_lit0(NULL, "json", "output in JSON format");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = json;
    argtable[2] = end;
    argtable[3] = NULL;

    fprintf(out, "Usage: localdate ");
    arg_print_syntax(out, argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the current local date in YYYY-MM-DD format.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-20s %s\n");

    arg_freetable(argtable, 3);
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
