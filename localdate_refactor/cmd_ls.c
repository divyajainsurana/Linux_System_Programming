 #include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_ls.h"

static const char *get_type(mode_t mode)
{
    if (S_ISDIR(mode)) return "dir";
    if (S_ISREG(mode)) return "file";
    if (S_ISLNK(mode)) return "link";
    return "other";
}

static struct arg_lit *ls_help_flag;
static struct arg_lit *ls_all_flag;
static struct arg_lit *ls_json_flag;
static struct arg_file *ls_paths;
static struct arg_end *ls_end;
static void *ls_argtable[6];

static void build_ls_argtable(void)
{
    ls_help_flag = arg_lit0("h", "help", "show help and exit");
    ls_all_flag  = arg_lit0("a", "all", "show hidden files");
    ls_json_flag = arg_lit0(NULL, "json", "output in JSON format");
    ls_paths     = arg_file0(NULL, NULL, "[PATH...]", "paths to list");
    ls_end       = arg_end(20);

    ls_argtable[0] = ls_help_flag;
    ls_argtable[1] = ls_all_flag;
    ls_argtable[2] = ls_json_flag;
    ls_argtable[3] = ls_paths;
    ls_argtable[4] = ls_end;
    ls_argtable[5] = NULL;
}

int ls_run(int argc, char **argv)
{
    build_ls_argtable();

    int nerrors = arg_parse(argc, argv, ls_argtable);

    if (ls_help_flag->count > 0) {
        ls_print_usage(stdout);
        arg_freetable(ls_argtable, 4);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, ls_end, "ls");
        ls_print_usage(stderr);
        arg_freetable(ls_argtable, 4);
        return 1;
    }

    const char *path = ".";
    if (ls_paths->count > 0) {
        path = ls_paths->filename[0];
    }

    DIR *dir = opendir(path);
    if (!dir) {
        perror("ls");
        arg_freetable(ls_argtable, 4);
        return 1;
    }

    struct dirent *entry;
int first = 1;

    /* 👇 STEP 4 STARTS HERE */
    if (ls_json_flag->count > 0) {
        printf("[\n");
    }

    while ((entry = readdir(dir)) != NULL) {

        if (!ls_all_flag->count && entry->d_name[0] == '.') {
            continue;
        }

        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) != 0) {
            continue;
        }

        if (ls_json_flag->count > 0) {

            if (!first) {
                printf(",\n");
            }

            printf("  {\"name\":\"%s\",\"type\":\"%s\",\"size\":%lld,\"mtime\":%lld}",
                   entry->d_name,
                   get_type(st.st_mode),
                   (long long)st.st_size,
                   (long long)st.st_mtime);

            first = 0;

        } else {
            printf("%s\n", entry->d_name);
        }
    }

    if (ls_json_flag->count > 0) {
        printf("\n]\n");
    }

    closedir(dir);
    arg_freetable(ls_argtable, 5);
    return 0;
}

void ls_print_usage(FILE *out)
{
    build_ls_argtable();

    fprintf(out, "Usage: ls ");
    arg_print_syntax(out, ls_argtable, "\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, ls_argtable, "  %-20s %s\n");

    arg_freetable(ls_argtable, 5);
}

cmd_spec_t cmd_ls_spec = {
    .name = "ls",
    .summary = "list directory contents",
    .long_help = "List files in a directory (default: current directory).",
    .run = ls_run,
    .print_usage = ls_print_usage,
};

void register_ls_command(void)
{
    register_command(&cmd_ls_spec);
}
