#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_mkdir.h"
#include "json_utils.h"

static struct arg_lit *mkdir_help_flag;
static struct arg_lit *mkdir_json_flag;
static struct arg_lit *mkdir_parents_flag;
static struct arg_lit *mkdir_verbose_flag;
static struct arg_lit *mkdir_dry_run_flag;
static struct arg_str *mkdir_mode_value;
static struct arg_file *mkdir_dirs;
static struct arg_end *mkdir_end;
static void *mkdir_argtable[9];

static void build_mkdir_argtable(int max_dirs)
{
    mkdir_help_flag = arg_lit0("h", "help", "show help and exit");
    mkdir_json_flag = arg_lit0(NULL, "json", "output in JSON format");
    mkdir_parents_flag = arg_lit0("p", "parents", "create parent directories as needed");
    mkdir_verbose_flag = arg_lit0("v", "verbose", "print created directories");
    mkdir_dry_run_flag = arg_lit0(NULL, "dry-run", "show what would be created");
    mkdir_mode_value = arg_str0("m", "mode", "MODE", "set permissions in octal");
    mkdir_dirs = arg_filen(NULL, NULL, "<DIR...>", 1, max_dirs, "directories to create");
    mkdir_end = arg_end(20);

    mkdir_argtable[0] = mkdir_help_flag;
    mkdir_argtable[1] = mkdir_json_flag;
    mkdir_argtable[2] = mkdir_parents_flag;
    mkdir_argtable[3] = mkdir_verbose_flag;
    mkdir_argtable[4] = mkdir_dry_run_flag;
    mkdir_argtable[5] = mkdir_mode_value;
    mkdir_argtable[6] = mkdir_dirs;
    mkdir_argtable[7] = mkdir_end;
    mkdir_argtable[8] = NULL;
}

static int path_is_directory(const char *path)
{
    struct stat st;

    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int create_one_directory(const char *path, int parents, mode_t mode,
                                int dry_run, int verbose)
{
    if (dry_run) {
        fprintf(stderr, "would create %s\n", path);
        return 0;
    }

    if (mkdir(path, mode) == 0) {
        if (verbose) {
            printf("created %s\n", path);
        }
        return 0;
    }

    if (parents && errno == EEXIST && path_is_directory(path)) {
        return 0;
    }

    fprintf(stderr, "mkdir: %s: %s\n", path, strerror(errno));
    return 1;
}

static int create_parent_directories(const char *path, mode_t mode, int dry_run, int verbose)
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
        if (partial[0] != '\0' && !path_is_directory(partial)) {
            if (dry_run) {
                fprintf(stderr, "would create %s\n", partial);
            } else if (mkdir(partial, mode) == 0) {
                if (verbose) {
                    printf("created %s\n", partial);
                }
            } else {
            if (errno != EEXIST || !path_is_directory(partial)) {
                fprintf(stderr, "mkdir: %s: %s\n", partial, strerror(errno));
                return 1;
            }
            }
        }
        partial[index] = '/';
    }

    return create_one_directory(partial, 1, mode, dry_run, verbose);
}

int mkdir_run(int argc, char **argv)
{
    int index;
    int status = 0;
    int parents;
    int nerrors;
    mode_t mode = 0777;
    int dry_run;
    int verbose;

    build_mkdir_argtable(argc + 2);

    nerrors = arg_parse(argc, argv, mkdir_argtable);

    if (mkdir_help_flag->count > 0) {
        mkdir_print_usage(stdout);
        arg_freetable(mkdir_argtable, 8);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, mkdir_end, "mkdir");
        mkdir_print_usage(stderr);
        arg_freetable(mkdir_argtable, 8);
        return 1;
    }

    parents = mkdir_parents_flag->count > 0;
    dry_run = mkdir_dry_run_flag->count > 0;
    verbose = mkdir_verbose_flag->count > 0 && mkdir_json_flag->count == 0;

    if (mkdir_mode_value->count > 0) {
        char *endptr;
        long parsed = strtol(mkdir_mode_value->sval[0], &endptr, 8);
        if (*endptr != '\0' || parsed < 0 || parsed > 07777) {
            fprintf(stderr, "mkdir: invalid mode: %s\n", mkdir_mode_value->sval[0]);
            arg_freetable(mkdir_argtable, 8);
            return 1;
        }
        mode = (mode_t)parsed;
    }

    if (mkdir_json_flag->count > 0) {
        printf("{\"command\":\"mkdir\",\"parents\":%s,\"dry_run\":%s,\"mode\":\"%03o\",\"directories\":[",
               parents ? "true" : "false",
               dry_run ? "true" : "false",
               (unsigned int)mode);
    }
    for (index = 0; index < mkdir_dirs->count; index++) {
        if (parents) {
            if (create_parent_directories(mkdir_dirs->filename[index], mode, dry_run, verbose) != 0) {
                status = 1;
            } else if (mkdir_json_flag->count > 0) {
                if (index > 0) {
                    putchar(',');
                }
                json_print_string(stdout, mkdir_dirs->filename[index]);
            }
        } else if (create_one_directory(mkdir_dirs->filename[index], 0, mode, dry_run, verbose) != 0) {
            status = 1;
        } else if (mkdir_json_flag->count > 0) {
            if (index > 0) {
                putchar(',');
            }
            json_print_string(stdout, mkdir_dirs->filename[index]);
        }
    }
    if (mkdir_json_flag->count > 0) {
        printf("],\"success\":%s}\n", status == 0 ? "true" : "false");
    }

    arg_freetable(mkdir_argtable, 8);
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

    arg_freetable(mkdir_argtable, 8);
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
