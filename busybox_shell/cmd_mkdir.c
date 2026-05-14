#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_mkdir.h"

static struct arg_lit *mkdir_help_flag;
static struct arg_lit *mkdir_parents_flag;
static struct arg_file *mkdir_dirs;
static struct arg_end *mkdir_end;
static void *mkdir_argtable[5];

static void build_mkdir_argtable(int max_dirs)
{
    mkdir_help_flag = arg_lit0("h", "help", "show help and exit");
    mkdir_parents_flag = arg_lit0("p", "parents", "create parent directories as needed");
    mkdir_dirs = arg_filen(NULL, NULL, "<DIR...>", 1, max_dirs, "directories to create");
    mkdir_end = arg_end(20);

    mkdir_argtable[0] = mkdir_help_flag;
    mkdir_argtable[1] = mkdir_parents_flag;
    mkdir_argtable[2] = mkdir_dirs;
    mkdir_argtable[3] = mkdir_end;
    mkdir_argtable[4] = NULL;
}

static int path_is_directory(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int create_one_directory(const char *path, int parents)
{
    if (mkdir(path, 0777) == 0) {
        return 0;
    }

    if (parents && errno == EEXIST && path_is_directory(path)) {
        return 0;
    }

    fprintf(stderr, "mkdir: %s: %s\n", path, strerror(errno));
    return 1;
}

static int create_parent_directories(const char *path)
{
    char partial[4096];
    size_t len;
    size_t index;

    len = strlen(path);
    if (len == 0 || len >= sizeof(partial)) {
        fprintf(stderr, "mkdir: %s: invalid path\n", path);
        return 1;
    }

    strcpy(partial, path);

    for (index = 1; index < len; index++) {
        if (partial[index] != '/') {
            continue;
        }

        partial[index] = '\0';
        if (partial[0] != '\0' && mkdir(partial, 0777) != 0) {
            if (errno != EEXIST || !path_is_directory(partial)) {
                fprintf(stderr, "mkdir: %s: %s\n", partial, strerror(errno));
                return 1;
            }
        }
        partial[index] = '/';
    }

    return create_one_directory(partial, 1);
}

int mkdir_run(int argc, char **argv)
{
    int index;
    int status = 0;
    int parents;
    int nerrors;

    build_mkdir_argtable(argc + 2);

    nerrors = arg_parse(argc, argv, mkdir_argtable);

    if (mkdir_help_flag->count > 0) {
        mkdir_print_usage(stdout);
        arg_freetable(mkdir_argtable, 4);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, mkdir_end, "mkdir");
        mkdir_print_usage(stderr);
        arg_freetable(mkdir_argtable, 4);
        return 1;
    }

    parents = mkdir_parents_flag->count > 0;

    for (index = 0; index < mkdir_dirs->count; index++) {
        if (parents) {
            if (create_parent_directories(mkdir_dirs->filename[index]) != 0) {
                status = 1;
            }
        } else if (create_one_directory(mkdir_dirs->filename[index], 0) != 0) {
            status = 1;
        }
    }

    arg_freetable(mkdir_argtable, 4);
    return status;
}

void mkdir_print_usage(FILE *out)
{
    build_mkdir_argtable(100);

    fprintf(out, "Usage: mkdir ");
    arg_print_syntax(out, mkdir_argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Create one or more directories.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, mkdir_argtable, "  %-20s %s\n");

    arg_freetable(mkdir_argtable, 4);
}

cmd_spec_t cmd_mkdir_spec = {
    .name = "mkdir",
    .summary = "create directories",
    .long_help = "Create one or more directories.",
    .run = mkdir_run,
    .print_usage = mkdir_print_usage,
};

void register_mkdir_command(void)
{
    register_command(&cmd_mkdir_spec);
}
