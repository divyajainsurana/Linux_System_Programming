#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_cp.h"
#include "json_utils.h"

static int copy_file(const char *source, const char *destination)
{
    FILE *in;
    FILE *out;
    char buffer[4096];
    size_t nread;
    int status = 0;

    in = fopen(source, "rb");
    if (in == NULL) {
        fprintf(stderr, "cp: %s: %s\n", source, strerror(errno));
        return 1;
    }

    out = fopen(destination, "wb");
    if (out == NULL) {
        fprintf(stderr, "cp: %s: %s\n", destination, strerror(errno));
        fclose(in);
        return 1;
    }

    while ((nread = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, nread, out) != nread) {
            fprintf(stderr, "cp: %s: %s\n", destination, strerror(errno));
            status = 1;
            break;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "cp: %s: %s\n", source, strerror(errno));
        status = 1;
    }

    if (fclose(out) != 0) {
        fprintf(stderr, "cp: %s: %s\n", destination, strerror(errno));
        status = 1;
    }
    if (fclose(in) != 0) {
        fprintf(stderr, "cp: %s: %s\n", source, strerror(errno));
        status = 1;
    }

    return status;
}

int cp_run(int argc, char **argv)
{
    int json = 0;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        cp_print_usage(stdout);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json = 1;
        argv++;
        argc--;
    }

    if (argc != 3) {
        fprintf(stderr, "cp: expected SOURCE and DEST\n");
        cp_print_usage(stderr);
        return 1;
    }

    if (copy_file(argv[1], argv[2]) != 0) {
        return 1;
    }

    if (json) {
        printf("{\"command\":\"cp\",\"source\":");
        json_print_string(stdout, argv[1]);
        printf(",\"destination\":");
        json_print_string(stdout, argv[2]);
        printf(",\"copied\":true}\n");
    }

    return 0;
}

void cp_print_usage(FILE *out)
{
    fprintf(out, "Usage: cp [--json] SOURCE DEST\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Copy a file from SOURCE to DEST.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
}

cmd_spec_t cmd_cp_spec = {
    .name = "cp",
    .summary = "copy files",
    .long_help = "Copy a file from SOURCE to DEST.",
    .run = cp_run,
    .print_usage = cp_print_usage,
};

void register_cp_command(void)
{
    register_command(&cmd_cp_spec);
}
