#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmd_procinfo.h"
#include "cmd_spec.h"

int procinfo_run(int argc, char **argv)
{
    int json = 0;
    pid_t pid;
    pid_t ppid;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        procinfo_print_usage(stdout);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json = 1;
    }

    if ((!json && argc > 1) || (json && argc > 2)) {
        fprintf(stderr, "procinfo: too many arguments\n");
        procinfo_print_usage(stderr);
        return 1;
    }

    pid = getpid();
    ppid = getppid();

    if (json) {
        printf("{\"pid\":%lu,\"ppid\":%lu}\n",
               (unsigned long)pid,
               (unsigned long)ppid);
    } else {
        printf("pid=%lu ppid=%lu\n",
               (unsigned long)pid,
               (unsigned long)ppid);
    }

    return ferror(stdout) ? 1 : 0;
}

void procinfo_print_usage(FILE *out)
{
    fprintf(out, "Usage: procinfo [--json]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print this command process id and parent process id.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
}

cmd_spec_t cmd_procinfo_spec = {
    .name = "procinfo",
    .summary = "print process identifiers",
    .long_help = "Print this command process id and parent process id.",
    .run = procinfo_run,
    .print_usage = procinfo_print_usage,
};

void register_procinfo_command(void)
{
    register_command(&cmd_procinfo_spec);
}
