#include <dirent.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_ls.h"
#include "json_utils.h"

struct ls_options {
    int show_all;
    int long_format;
    int recursive;
    int sort_size;
    int sort_time;
    int reverse;
    int color;
    int json;
};

struct ls_entry {
    char name[1024];
    char path[2048];
    struct stat st;
};

static const char *get_type(mode_t mode)
{
    if (S_ISDIR(mode)) return "dir";
    if (S_ISREG(mode)) return "file";
    if (S_ISLNK(mode)) return "link";
    return "other";
}

static void mode_string(mode_t mode, char out[11])
{
    out[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    out[1] = mode & S_IRUSR ? 'r' : '-';
    out[2] = mode & S_IWUSR ? 'w' : '-';
    out[3] = mode & S_IXUSR ? 'x' : '-';
    out[4] = mode & S_IRGRP ? 'r' : '-';
    out[5] = mode & S_IWGRP ? 'w' : '-';
    out[6] = mode & S_IXGRP ? 'x' : '-';
    out[7] = mode & S_IROTH ? 'r' : '-';
    out[8] = mode & S_IWOTH ? 'w' : '-';
    out[9] = mode & S_IXOTH ? 'x' : '-';
    out[10] = '\0';
}

static int compare_name(const void *left, const void *right)
{
    const struct ls_entry *a = left;
    const struct ls_entry *b = right;

    return strcmp(a->name, b->name);
}

static int compare_size(const void *left, const void *right)
{
    const struct ls_entry *a = left;
    const struct ls_entry *b = right;

    if (a->st.st_size == b->st.st_size) {
        return compare_name(left, right);
    }
    return a->st.st_size < b->st.st_size ? 1 : -1;
}

static int compare_time(const void *left, const void *right)
{
    const struct ls_entry *a = left;
    const struct ls_entry *b = right;

    if (a->st.st_mtime == b->st.st_mtime) {
        return compare_name(left, right);
    }
    return a->st.st_mtime < b->st.st_mtime ? 1 : -1;
}

static void reverse_entries(struct ls_entry *entries, size_t count)
{
    size_t left = 0;
    size_t right = count == 0 ? 0 : count - 1;

    while (left < right) {
        struct ls_entry tmp = entries[left];
        entries[left] = entries[right];
        entries[right] = tmp;
        left++;
        right--;
    }
}

static int add_entry(struct ls_entry **entries, size_t *count, size_t *capacity,
                     const char *dir_path, const char *name)
{
    struct ls_entry *resized;
    int written;

    if (*count == *capacity) {
        size_t next = *capacity == 0 ? 32 : *capacity * 2;
        resized = realloc(*entries, next * sizeof(**entries));
        if (resized == NULL) {
            fprintf(stderr, "ls: out of memory\n");
            return 1;
        }
        *entries = resized;
        *capacity = next;
    }

    snprintf((*entries)[*count].name, sizeof((*entries)[*count].name), "%s", name);
    written = snprintf((*entries)[*count].path, sizeof((*entries)[*count].path),
                       "%s/%s", dir_path, name);
    if (written < 0 || (size_t)written >= sizeof((*entries)[*count].path)) {
        fprintf(stderr, "ls: %s/%s: path too long\n", dir_path, name);
        return 1;
    }

    if (lstat((*entries)[*count].path, &(*entries)[*count].st) != 0) {
        fprintf(stderr, "ls: %s: %s\n", (*entries)[*count].path, strerror(errno));
        return 1;
    }

    (*count)++;
    return 0;
}

static void print_name(const struct ls_entry *entry, int color)
{
    if (color && S_ISDIR(entry->st.st_mode)) {
        printf("\033[34m%s\033[0m\n", entry->name);
    } else if (color && (entry->st.st_mode & S_IXUSR)) {
        printf("\033[32m%s\033[0m\n", entry->name);
    } else {
        printf("%s\n", entry->name);
    }
}

static void print_long_entry(const struct ls_entry *entry)
{
    char mode[11];
    char timebuf[32];
    struct passwd *pw = getpwuid(entry->st.st_uid);
    struct group *gr = getgrgid(entry->st.st_gid);
    struct tm *tm = localtime(&entry->st.st_mtime);

    mode_string(entry->st.st_mode, mode);
    if (tm != NULL) {
        strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);
    } else {
        snprintf(timebuf, sizeof(timebuf), "?");
    }

    printf("%s %3lu %-8s %-8s %8lld %s %s\n",
           mode,
           (unsigned long)entry->st.st_nlink,
           pw != NULL ? pw->pw_name : "?",
           gr != NULL ? gr->gr_name : "?",
           (long long)entry->st.st_size,
           timebuf,
           entry->name);
}

static void print_json_entry(const struct ls_entry *entry, int *first)
{
    if (!*first) {
        printf(",\n");
    }

    printf("  {\"name\":");
    json_print_string(stdout, entry->name);
    printf(",\"path\":");
    json_print_string(stdout, entry->path);
    printf(",\"type\":");
    json_print_string(stdout, get_type(entry->st.st_mode));
    printf(",\"size\":%lld,\"mtime\":%lld}",
           (long long)entry->st.st_size,
           (long long)entry->st.st_mtime);

    *first = 0;
}

static int list_path(const char *path, const struct ls_options *opts,
                     int print_header, int *json_first)
{
    DIR *dir;
    struct dirent *entry;
    struct ls_entry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t index;
    int status = 0;

    dir = opendir(path);
    if (dir == NULL) {
        struct stat st;
        struct ls_entry single;

        if (lstat(path, &st) != 0) {
            fprintf(stderr, "ls: %s: %s\n", path, strerror(errno));
            return 1;
        }
        snprintf(single.name, sizeof(single.name), "%s", path);
        snprintf(single.path, sizeof(single.path), "%s", path);
        single.st = st;

        if (opts->json) {
            print_json_entry(&single, json_first);
        } else if (opts->long_format) {
            print_long_entry(&single);
        } else {
            print_name(&single, opts->color);
        }
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (!opts->show_all && entry->d_name[0] == '.') {
            continue;
        }
        if (add_entry(&entries, &count, &capacity, path, entry->d_name) != 0) {
            status = 1;
        }
    }
    closedir(dir);

    if (opts->sort_size) {
        qsort(entries, count, sizeof(*entries), compare_size);
    } else if (opts->sort_time) {
        qsort(entries, count, sizeof(*entries), compare_time);
    } else {
        qsort(entries, count, sizeof(*entries), compare_name);
    }
    if (opts->reverse) {
        reverse_entries(entries, count);
    }

    if (!opts->json && print_header) {
        printf("%s:\n", path);
    }

    for (index = 0; index < count; index++) {
        if (opts->json) {
            print_json_entry(&entries[index], json_first);
        } else if (opts->long_format) {
            print_long_entry(&entries[index]);
        } else {
            print_name(&entries[index], opts->color);
        }
    }

    if (opts->recursive) {
        for (index = 0; index < count; index++) {
            if (!S_ISDIR(entries[index].st.st_mode) ||
                strcmp(entries[index].name, ".") == 0 ||
                strcmp(entries[index].name, "..") == 0) {
                continue;
            }
            if (!opts->json) {
                printf("\n");
            }
            if (list_path(entries[index].path, opts, 1, json_first) != 0) {
                status = 1;
            }
        }
    }

    free(entries);
    return status;
}

int ls_run(int argc, char **argv)
{
    struct ls_options opts = {0, 0, 0, 0, 0, 0, 0, 0};
    const char *paths[256];
    int path_count = 0;
    int index;
    int json_first = 1;
    int status = 0;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            ls_print_usage(stdout);
            return 0;
        } else if (strcmp(argv[index], "-a") == 0 || strcmp(argv[index], "--all") == 0) {
            opts.show_all = 1;
        } else if (strcmp(argv[index], "-l") == 0) {
            opts.long_format = 1;
        } else if (strcmp(argv[index], "-R") == 0) {
            opts.recursive = 1;
        } else if (strcmp(argv[index], "-S") == 0) {
            opts.sort_size = 1;
            opts.sort_time = 0;
        } else if (strcmp(argv[index], "-t") == 0) {
            opts.sort_time = 1;
            opts.sort_size = 0;
        } else if (strcmp(argv[index], "-r") == 0) {
            opts.reverse = 1;
        } else if (strcmp(argv[index], "--color") == 0) {
            opts.color = 1;
        } else if (strcmp(argv[index], "--json") == 0) {
            opts.json = 1;
        } else if (argv[index][0] == '-') {
            fprintf(stderr, "ls: invalid option: %s\n", argv[index]);
            ls_print_usage(stderr);
            return 1;
        } else {
            if (path_count < (int) (sizeof(paths) / sizeof(paths[0]))) {
                paths[path_count++] = argv[index];
            }
        }
    }

    if (path_count == 0) {
        paths[path_count++] = ".";
    }

    if (opts.json) {
        printf("[\n");
    }
    for (index = 0; index < path_count; index++) {
        if (list_path(paths[index], &opts, path_count > 1, &json_first) != 0) {
            status = 1;
        }
    }
    if (opts.json) {
        printf("\n]\n");
    }
    return status;
}

void ls_print_usage(FILE *out)
{
    fprintf(out, "Usage: ls [-a] [-l] [-R] [-S] [-t] [-r] [--color] [--json] [PATH...]\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  List files in a directory (default: current directory).\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-a, --all", "show hidden files");
    fprintf(out, "  %-20s %s\n", "-l", "use long listing format");
    fprintf(out, "  %-20s %s\n", "-R", "list directories recursively");
    fprintf(out, "  %-20s %s\n", "-S", "sort by size");
    fprintf(out, "  %-20s %s\n", "-t", "sort by modification time");
    fprintf(out, "  %-20s %s\n", "-r", "reverse sort order");
    fprintf(out, "  %-20s %s\n", "--color", "colorize file names");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
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
