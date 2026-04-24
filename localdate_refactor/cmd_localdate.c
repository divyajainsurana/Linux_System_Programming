#include "cmd_localdate.h"

#include <stdio.h>
#include <time.h>
#include "argtable3.h"

/*
 * This function is expected to be provided by the shell registry.
 * When building only the standalone binary, do not compile/link this
 * function unless your shell registry object is also linked.
 */
extern void register_command(const cmd_spec_t *spec);

static void build_localdate_argtable(struct arg_lit **help,
                                     struct arg_end **end,
                                     void ***argtable_out)
{
    *help = arg_lit0("h", "help", "show help and exit");
    *end = arg_end(20);

    static void *argtable[3];
    argtable[0] = *help;
    argtable[1] = *end;
    argtable[2] = NULL;

    *argtable_out = argtable;
}

void localdate_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_end *end;
    void **argtable;

    build_localdate_argtable(&help, &end, &argtable);

    fprintf(out, "Usage: localdate ");
    arg_print_syntax(out, argtable, "\n");
    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-20s %s\n");
}

int localdate_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_end *end;
    void **argtable;

    build_localdate_argtable(&help, &end, &argtable);

    int nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        localdate_print_usage(stdout);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]) - 1);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stdout, end, "localdate");
        localdate_print_usage(stdout);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]) - 1);
        return 1;
    }

    time_t t = time(NULL);
    struct tm *local = localtime(&t);

    if (local == NULL) {
        fprintf(stderr, "localdate: failed to get local time\n");
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]) - 1);
        return 1;
    }

    printf("Local Date: %04d-%02d-%02d\n",
           local->tm_year + 1900,
           local->tm_mon + 1,
           local->tm_mday);

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]) - 1);
    return 0;
}

cmd_spec_t cmd_localdate_spec = {
    .name = "localdate",
    .summary = "print the current local date",
    .long_help = "Print the current local date in YYYY-MM-DD format using the system local timezone.",
    .run = localdate_run,
    .print_usage = localdate_print_usage,
};

void register_localdate_command(void)
{
    register_command(&cmd_localdate_spec);
}
