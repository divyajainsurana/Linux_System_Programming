#include <errno.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_whoami.h"

int whoami_run(int argc, char **argv)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *argtable[3];
    struct passwd *user_info;
    int nerrors;

    help = arg_lit0("h", "help", "show help and exit");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = end;
    argtable[2] = NULL;

    nerrors = arg_parse(argc, argv, argtable);

    if (help->count > 0) {
        whoami_print_usage(stdout);
        arg_freetable(argtable, 2);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, end, "whoami");
        whoami_print_usage(stderr);
        arg_freetable(argtable, 2);
        return 1;
    }

    errno = 0;
    user_info = getpwuid(getuid());
    if (user_info == NULL) {
        if (errno != 0) {
            fprintf(stderr, "whoami: %s\n", strerror(errno));
        } else {
            fprintf(stderr, "whoami: cannot find username for current user\n");
        }
        arg_freetable(argtable, 2);
        return 1;
    }

    printf("%s\n", user_info->pw_name);

    arg_freetable(argtable, 2);
    return 0;
}

void whoami_print_usage(FILE *out)
{
    struct arg_lit *help;
    struct arg_end *end;
    void *argtable[3];

    help = arg_lit0("h", "help", "show help and exit");
    end = arg_end(20);

    argtable[0] = help;
    argtable[1] = end;
    argtable[2] = NULL;

    fprintf(out, "Usage: whoami ");
    arg_print_syntax(out, argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the username of the current user.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, argtable, "  %-20s %s\n");

    arg_freetable(argtable, 2);
}

cmd_spec_t cmd_whoami_spec = {
    .name = "whoami",
    .summary = "print the current username",
    .long_help = "Print the username of the current user.",
    .run = whoami_run,
    .print_usage = whoami_print_usage,
};

void register_whoami_command(void)
{
    register_command(&cmd_whoami_spec);
}
