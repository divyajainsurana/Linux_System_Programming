#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#include "cmd_spec.h"
#include "cmd_uname.h"

static void print_field(const char *value, int *printed)
{
    if (*printed) {
        putchar(' ');
    }
    fputs(value, stdout);
    *printed = 1;
}

int uname_run(int argc, char **argv)
{
    struct utsname info;
    int show_sysname = 0;
    int show_nodename = 0;
    int show_release = 0;
    int show_version = 0;
    int show_machine = 0;
    int printed = 0;
    int index;

    if (uname(&info) != 0) {
        perror("uname");
        return 1;
    }

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            uname_print_usage(stdout);
            return 0;
        } else if (strcmp(argv[index], "-a") == 0 || strcmp(argv[index], "--all") == 0) {
            show_sysname = 1;
            show_nodename = 1;
            show_release = 1;
            show_version = 1;
            show_machine = 1;
        } else if (strcmp(argv[index], "-s") == 0) {
            show_sysname = 1;
        } else if (strcmp(argv[index], "-n") == 0) {
            show_nodename = 1;
        } else if (strcmp(argv[index], "-r") == 0) {
            show_release = 1;
        } else if (strcmp(argv[index], "-v") == 0) {
            show_version = 1;
        } else if (strcmp(argv[index], "-m") == 0) {
            show_machine = 1;
        } else {
            fprintf(stderr, "uname: invalid option: %s\n", argv[index]);
            uname_print_usage(stderr);
            return 1;
        }
    }

    if (!show_sysname && !show_nodename && !show_release &&
        !show_version && !show_machine) {
        show_sysname = 1;
    }

    if (show_sysname) {
        print_field(info.sysname, &printed);
    }
    if (show_nodename) {
        print_field(info.nodename, &printed);
    }
    if (show_release) {
        print_field(info.release, &printed);
    }
    if (show_version) {
        print_field(info.version, &printed);
    }
    if (show_machine) {
        print_field(info.machine, &printed);
    }
    putchar('\n');

    return 0;
}

void uname_print_usage(FILE *out)
{
    fprintf(out, "Usage: uname [-a] [-s] [-n] [-r] [-v] [-m]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print system information.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-a, --all", "print all available information");
    fprintf(out, "  %-20s %s\n", "-s", "print kernel name");
    fprintf(out, "  %-20s %s\n", "-n", "print network node hostname");
    fprintf(out, "  %-20s %s\n", "-r", "print kernel release");
    fprintf(out, "  %-20s %s\n", "-v", "print kernel version");
    fprintf(out, "  %-20s %s\n", "-m", "print machine hardware name");
}

cmd_spec_t cmd_uname_spec = {
    .name = "uname",
    .summary = "print system information",
    .long_help = "Print system information.",
    .run = uname_run,
    .print_usage = uname_print_usage,
};

void register_uname_command(void)
{
    register_command(&cmd_uname_spec);
}
