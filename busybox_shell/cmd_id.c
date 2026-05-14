#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_id.h"

int id_run(int argc, char **argv)
{
    uid_t uid;
    gid_t gid;
    struct passwd *user_info;
    struct group *group_info;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        id_print_usage(stdout);
        return 0;
    }

    if (argc > 1) {
        fprintf(stderr, "id: too many arguments\n");
        id_print_usage(stderr);
        return 1;
    }

    uid = getuid();
    gid = getgid();
    user_info = getpwuid(uid);
    group_info = getgrgid(gid);

    printf("uid=%lu", (unsigned long)uid);
    if (user_info != NULL) {
        printf("(%s)", user_info->pw_name);
    }

    printf(" gid=%lu", (unsigned long)gid);
    if (group_info != NULL) {
        printf("(%s)", group_info->gr_name);
    }

    printf("\n");
    return 0;
}

void id_print_usage(FILE *out)
{
    fprintf(out, "Usage: id [-h]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print the current user and group identifiers.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
}

cmd_spec_t cmd_id_spec = {
    .name = "id",
    .summary = "print user and group identifiers",
    .long_help = "Print the current user and group identifiers.",
    .run = id_run,
    .print_usage = id_print_usage,
};

void register_id_command(void)
{
    register_command(&cmd_id_spec);
}
