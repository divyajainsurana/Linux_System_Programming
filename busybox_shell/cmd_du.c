#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_du.h"

static unsigned long long blocks_to_kb(unsigned long long blocks)
{
    return (blocks * 512ULL + 1023ULL) / 1024ULL;
}

static int add_path(const char *parent, const char *name, char *out, size_t out_size)
{
    int written;

    written = snprintf(out, out_size, "%s/%s", parent, name);
    return written < 0 || (size_t)written >= out_size;
}

static int disk_usage(const char *path, unsigned long long *blocks)
{
    struct stat st;
    DIR *dir;
    struct dirent *entry;
    unsigned long long total;

    if (lstat(path, &st) != 0) {
        fprintf(stderr, "du: %s: %s\n", path, strerror(errno));
        return 1;
    }

    total = (unsigned long long)st.st_blocks;

    if (S_ISDIR(st.st_mode)) {
        dir = opendir(path);
        if (dir == NULL) {
            fprintf(stderr, "du: %s: %s\n", path, strerror(errno));
            return 1;
        }

        while ((entry = readdir(dir)) != NULL) {
            char child[4096];
            unsigned long long child_blocks = 0;

            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            if (add_path(path, entry->d_name, child, sizeof(child)) != 0) {
                fprintf(stderr, "du: %s/%s: path too long\n", path, entry->d_name);
                closedir(dir);
                return 1;
            }

            if (disk_usage(child, &child_blocks) != 0) {
                closedir(dir);
                return 1;
            }
            total += child_blocks;
        }

        if (closedir(dir) != 0) {
            fprintf(stderr, "du: %s: %s\n", path, strerror(errno));
            return 1;
        }
    }

    *blocks = total;
    return 0;
}

int du_run(int argc, char **argv)
{
    int index = 1;
    int status = 0;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        du_print_usage(stdout);
        return 0;
    }

    if (index == argc) {
        unsigned long long blocks = 0;

        if (disk_usage(".", &blocks) != 0) {
            return 1;
        }
        printf("%llu\t.\n", blocks_to_kb(blocks));
        return 0;
    }

    for (; index < argc; index++) {
        unsigned long long blocks = 0;

        if (disk_usage(argv[index], &blocks) != 0) {
            status = 1;
            continue;
        }
        printf("%llu\t%s\n", blocks_to_kb(blocks), argv[index]);
    }

    return status;
}

void du_print_usage(FILE *out)
{
    fprintf(out, "Usage: du [PATH...]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Show disk usage in kilobytes.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
}

cmd_spec_t cmd_du_spec = {
    .name = "du",
    .summary = "show disk usage",
    .long_help = "Show disk usage in kilobytes.",
    .run = du_run,
    .print_usage = du_print_usage,
};

void register_du_command(void)
{
    register_command(&cmd_du_spec);
}
