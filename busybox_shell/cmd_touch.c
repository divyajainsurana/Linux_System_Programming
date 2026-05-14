#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_touch.h"

static struct arg_lit *touch_help_flag;
static struct arg_file *touch_files;
static struct arg_end *touch_end;
static void *touch_argtable[4];

static void build_touch_argtable(int max_files)
{
    touch_help_flag = arg_lit0("h", "help", "show help and exit");
    touch_files = arg_filen(NULL, NULL, "<FILE...>", 1, max_files, "files to update or create");
    touch_end = arg_end(20);

    touch_argtable[0] = touch_help_flag;
    touch_argtable[1] = touch_files;
    touch_argtable[2] = touch_end;
    touch_argtable[3] = NULL;
}

static int touch_file(const char *path)
{
    int fd;

    if (utime(path, NULL) == 0) {
        return 0;
    }

    if (errno != ENOENT) {
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

    return 0;
}

int touch_run(int argc, char **argv)
{
    int index;
    int status = 0;
    int nerrors;

    build_touch_argtable(argc + 2);

    nerrors = arg_parse(argc, argv, touch_argtable);

    if (touch_help_flag->count > 0) {
        touch_print_usage(stdout);
        arg_freetable(touch_argtable, 3);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, touch_end, "touch");
        touch_print_usage(stderr);
        arg_freetable(touch_argtable, 3);
        return 1;
    }

    for (index = 0; index < touch_files->count; index++) {
        if (touch_file(touch_files->filename[index]) != 0) {
            status = 1;
        }
    }

    arg_freetable(touch_argtable, 3);
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

    arg_freetable(touch_argtable, 3);
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
