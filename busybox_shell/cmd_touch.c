#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_touch.h"
#include "json_utils.h"

static struct arg_lit *touch_help_flag;
static struct arg_lit *touch_json_flag;
static struct arg_lit *touch_no_create_flag;
static struct arg_lit *touch_access_flag;
static struct arg_lit *touch_modify_flag;
static struct arg_lit *touch_verbose_flag;
static struct arg_str *touch_time_value;
static struct arg_file *touch_files;
static struct arg_end *touch_end;
static void *touch_argtable[10];

static void build_touch_argtable(int max_files)
{
    touch_help_flag = arg_lit0("h", "help", "show help and exit");
    touch_json_flag = arg_lit0(NULL, "json", "output in JSON format");
    touch_no_create_flag = arg_lit0("c", "no-create", "do not create missing files");
    touch_access_flag = arg_lit0("a", NULL, "change only access time");
    touch_modify_flag = arg_lit0("m", NULL, "change only modification time");
    touch_verbose_flag = arg_lit0("v", "verbose", "print touched files");
    touch_time_value = arg_str0("t", NULL, "TIME", "use timestamp YYYYMMDDhhmm");
    touch_files = arg_filen(NULL, NULL, "<FILE...>", 1, max_files, "files to update or create");
    touch_end = arg_end(20);

    touch_argtable[0] = touch_help_flag;
    touch_argtable[1] = touch_json_flag;
    touch_argtable[2] = touch_no_create_flag;
    touch_argtable[3] = touch_access_flag;
    touch_argtable[4] = touch_modify_flag;
    touch_argtable[5] = touch_verbose_flag;
    touch_argtable[6] = touch_time_value;
    touch_argtable[7] = touch_files;
    touch_argtable[8] = touch_end;
    touch_argtable[9] = NULL;
}

static int parse_touch_time(const char *text, time_t *out)
{
    struct tm tm;
    int year;
    int month;
    int day;
    int hour;
    int minute;

    if (strlen(text) != 12 ||
        sscanf(text, "%4d%2d%2d%2d%2d", &year, &month, &day, &hour, &minute) != 5) {
        return 1;
    }

    memset(&tm, 0, sizeof(tm));
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_isdst = -1;

    *out = mktime(&tm);
    return *out == (time_t)-1;
}

static int touch_file(const char *path, int no_create, int access_only,
                      int modify_only, int use_custom_time, time_t custom_time)
{
    int fd;
    struct stat st;
    struct utimbuf times;

    if (stat(path, &st) == 0) {
        time_t new_time = use_custom_time ? custom_time : time(NULL);

        times.actime = modify_only ? st.st_atime : new_time;
        times.modtime = access_only ? st.st_mtime : new_time;
        if (!access_only && !modify_only) {
            times.actime = new_time;
            times.modtime = new_time;
        }

        if (utime(path, &times) != 0) {
            fprintf(stderr, "touch: %s: %s\n", path, strerror(errno));
            return 1;
        }
        return 0;
    }

    if (errno != ENOENT || no_create) {
        if (no_create && errno == ENOENT) {
            return 0;
        }
        fprintf(stderr, "touch: %s: %s\n", path, strerror(errno));
        return 1;
    }

    fd = open(path, O_WRONLY | O_CREAT, 0666);
    if (fd < 0) {
        fprintf(stderr, "touch: %s: %s\n", path, strerror(errno));
        return 1;
    }

    if (close(fd) != 0) {
        fprintf(stderr, "touch: %s: %s\n", path, strerror(errno));
        return 1;
    }

    if (use_custom_time || access_only || modify_only) {
        return touch_file(path, 1, access_only, modify_only, use_custom_time, custom_time);
    }

    return 0;
}

int touch_run(int argc, char **argv)
{
    int index;
    int status = 0;
    int nerrors;
    int use_custom_time = 0;
    time_t custom_time = 0;
    int access_only;
    int modify_only;

    build_touch_argtable(argc + 2);

    nerrors = arg_parse(argc, argv, touch_argtable);

    if (touch_help_flag->count > 0) {
        touch_print_usage(stdout);
        arg_freetable(touch_argtable, 9);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, touch_end, "touch");
        touch_print_usage(stderr);
        arg_freetable(touch_argtable, 9);
        return 1;
    }

    if (touch_time_value->count > 0) {
        use_custom_time = 1;
        if (parse_touch_time(touch_time_value->sval[0], &custom_time) != 0) {
            fprintf(stderr, "touch: invalid timestamp: %s\n", touch_time_value->sval[0]);
            arg_freetable(touch_argtable, 9);
            return 1;
        }
    }

    access_only = touch_access_flag->count > 0 && touch_modify_flag->count == 0;
    modify_only = touch_modify_flag->count > 0 && touch_access_flag->count == 0;

    if (touch_json_flag->count > 0) {
        printf("{\"command\":\"touch\",\"files\":[");
    }
    for (index = 0; index < touch_files->count; index++) {
        if (touch_file(touch_files->filename[index],
                       touch_no_create_flag->count > 0,
                       access_only,
                       modify_only,
                       use_custom_time,
                       custom_time) != 0) {
            status = 1;
        } else if (touch_json_flag->count > 0) {
            if (index > 0) {
                putchar(',');
            }
            json_print_string(stdout, touch_files->filename[index]);
        } else if (touch_verbose_flag->count > 0) {
            printf("touched %s\n", touch_files->filename[index]);
        }
    }
    if (touch_json_flag->count > 0) {
        printf("],\"success\":%s}\n", status == 0 ? "true" : "false");
    }

    arg_freetable(touch_argtable, 9);
    return status;
}

void touch_print_usage(FILE *out)
{
    build_touch_argtable(100);

    fprintf(out, "Usage: touch ");
    arg_print_syntax(out, touch_argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Create files if needed and update their access and modification timestamps.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, touch_argtable, "  %-20s %s\n");

    arg_freetable(touch_argtable, 9);
}

cmd_spec_t cmd_touch_spec = {
    .name = "touch",
    .summary = "create files or update timestamps",
    .long_help = "Create files if needed and update their access and modification timestamps.",
    .run = touch_run,
    .print_usage = touch_print_usage,
};

void register_touch_command(void)
{
    register_command(&cmd_touch_spec);
}
