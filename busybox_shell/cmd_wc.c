#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "argtable3/src/argtable3.h"
#include "cmd_spec.h"
#include "cmd_wc.h"
#include "json_utils.h"

struct wc_counts {
    unsigned long long lines;
    unsigned long long words;
    unsigned long long bytes;
};

static struct arg_lit *wc_help_flag;
static struct arg_lit *wc_lines_flag;
static struct arg_lit *wc_words_flag;
static struct arg_lit *wc_bytes_flag;
static struct arg_lit *wc_json_flag;
static struct arg_file *wc_files;
static struct arg_end *wc_end;
static void *wc_argtable[8];

static void build_wc_argtable(int max_files)
{
    wc_help_flag = arg_lit0("h", "help", "show help and exit");
    wc_lines_flag = arg_lit0("l", "lines", "print line count");
    wc_words_flag = arg_lit0("w", "words", "print word count");
    wc_bytes_flag = arg_lit0("c", "bytes", "print byte count");
    wc_json_flag = arg_lit0(NULL, "json", "output in JSON format");
    wc_files = arg_filen(NULL, NULL, "[FILE...]", 0, max_files, "files to count");
    wc_end = arg_end(20);

    wc_argtable[0] = wc_help_flag;
    wc_argtable[1] = wc_lines_flag;
    wc_argtable[2] = wc_words_flag;
    wc_argtable[3] = wc_bytes_flag;
    wc_argtable[4] = wc_json_flag;
    wc_argtable[5] = wc_files;
    wc_argtable[6] = wc_end;
    wc_argtable[7] = NULL;
}

static int count_stream(FILE *in, const char *name, struct wc_counts *counts)
{
    int ch;
    int in_word = 0;

    counts->lines = 0;
    counts->words = 0;
    counts->bytes = 0;

    while ((ch = fgetc(in)) != EOF) {
        counts->bytes++;

        if (ch == '\n') {
            counts->lines++;
        }

        if (isspace((unsigned char)ch)) {
            in_word = 0;
        } else if (!in_word) {
            counts->words++;
            in_word = 1;
        }
    }

    if (ferror(in)) {
        fprintf(stderr, "wc: %s: %s\n", name, strerror(errno));
        return 1;
    }

    return 0;
}

static void print_counts_json(const struct wc_counts *counts, const char *label)
{
    printf("{\"lines\":%llu,\"words\":%llu,\"bytes\":%llu",
           counts->lines,
           counts->words,
           counts->bytes);
    if (label != NULL) {
        printf(",\"path\":");
        json_print_string(stdout, label);
    }
    printf("}");
}

static int count_file(const char *path, struct wc_counts *counts)
{
    FILE *in;
    int status;

    if (strcmp(path, "-") == 0) {
        return count_stream(stdin, "standard input", counts);
    }

    in = fopen(path, "rb");
    if (in == NULL) {
        fprintf(stderr, "wc: %s: %s\n", path, strerror(errno));
        return 1;
    }

    status = count_stream(in, path, counts);

    if (fclose(in) != 0) {
        fprintf(stderr, "wc: %s: %s\n", path, strerror(errno));
        status = 1;
    }

    return status;
}

static void print_counts(
    const struct wc_counts *counts,
    const char *label,
    int show_lines,
    int show_words,
    int show_bytes
)
{
    if (show_lines) {
        printf("%8llu", counts->lines);
    }

    if (show_words) {
        printf("%8llu", counts->words);
    }

    if (show_bytes) {
        printf("%8llu", counts->bytes);
    }

    if (label != NULL) {
        printf(" %s", label);
    }

    printf("\n");
}

int wc_run(int argc, char **argv)
{
    struct wc_counts counts;
    struct wc_counts total = {0, 0, 0};
    int show_lines;
    int show_words;
    int show_bytes;
    int status = 0;
    int index;
    int nerrors;

    build_wc_argtable(argc + 2);

    nerrors = arg_parse(argc, argv, wc_argtable);

    if (wc_help_flag->count > 0) {
        wc_print_usage(stdout);
        arg_freetable(wc_argtable, 7);
        return 0;
    }

    if (nerrors > 0) {
        arg_print_errors(stderr, wc_end, "wc");
        wc_print_usage(stderr);
        arg_freetable(wc_argtable, 7);
        return 1;
    }

    show_lines = wc_lines_flag->count > 0;
    show_words = wc_words_flag->count > 0;
    show_bytes = wc_bytes_flag->count > 0;

    if (!show_lines && !show_words && !show_bytes) {
        show_lines = 1;
        show_words = 1;
        show_bytes = 1;
    }

    if (wc_files->count == 0) {
        status = count_stream(stdin, "standard input", &counts);
        if (status == 0) {
            if (wc_json_flag->count > 0) {
                printf("{\"files\":[");
                print_counts_json(&counts, NULL);
                printf("]}\n");
            } else {
                print_counts(&counts, NULL, show_lines, show_words, show_bytes);
            }
        }
    } else {
        if (wc_json_flag->count > 0) {
            printf("{\"files\":[");
        }
        for (index = 0; index < wc_files->count; index++) {
            if (count_file(wc_files->filename[index], &counts) != 0) {
                status = 1;
                continue;
            }

            total.lines += counts.lines;
            total.words += counts.words;
            total.bytes += counts.bytes;
            if (wc_json_flag->count > 0) {
                if (index > 0) {
                    putchar(',');
                }
                print_counts_json(&counts, wc_files->filename[index]);
            } else {
                print_counts(&counts, wc_files->filename[index], show_lines, show_words, show_bytes);
            }
        }

        if (wc_json_flag->count > 0) {
            printf("]");
            if (wc_files->count > 1) {
                printf(",\"total\":");
                print_counts_json(&total, NULL);
            }
            printf("}\n");
        } else if (wc_files->count > 1) {
            print_counts(&total, "total", show_lines, show_words, show_bytes);
        }
    }

    arg_freetable(wc_argtable, 7);
    return status;
}

void wc_print_usage(FILE *out)
{
    build_wc_argtable(100);

    fprintf(out, "Usage: wc ");
    arg_print_syntax(out, wc_argtable, "\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Count lines, words, and bytes from files or standard input.\n");

    fprintf(out, "\nOptions:\n");
    arg_print_glossary(out, wc_argtable, "  %-20s %s\n");

    arg_freetable(wc_argtable, 7);
}

cmd_spec_t cmd_wc_spec = {
    .name = "wc",
    .summary = "count lines, words, and bytes",
    .long_help = "Count lines, words, and bytes from files or standard input.",
    .run = wc_run,
    .print_usage = wc_print_usage,
};

void register_wc_command(void)
{
    register_command(&cmd_wc_spec);
}
