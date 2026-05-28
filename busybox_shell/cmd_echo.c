#include <stdio.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_echo.h"
#include "json_utils.h"

int echo_run(int argc, char **argv)
{
    int index = 1;
    int trailing_newline = 1;
    int json = 0;
    int interpret_escapes = 0;

    if (argc > 1 &&
        (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        echo_print_usage(stdout);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        json = 1;
        index = 2;
    }

    if (index < argc && strcmp(argv[index], "-n") == 0) {
        trailing_newline = 0;
        index++;
    }

    while (index < argc &&
           (strcmp(argv[index], "-e") == 0 || strcmp(argv[index], "-E") == 0)) {
        interpret_escapes = strcmp(argv[index], "-e") == 0;
        index++;
    }

    if (json) {
        int text_start = index;

        printf("{\"command\":\"echo\",\"text\":\"");
        for (; index < argc; index++) {
            if (index > text_start) {
                putchar(' ');
            }
            for (const char *ch = argv[index]; *ch != '\0'; ch++) {
                if (interpret_escapes && *ch == '\\' && ch[1] != '\0') {
                    ch++;
                    switch (*ch) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case 'b': putchar('\b'); break;
                    case '\\': putchar('\\'); break;
                    default:
                        putchar('\\');
                        putchar(*ch);
                        break;
                    }
                } else {
                    if (*ch == '"' || *ch == '\\') {
                        putchar('\\');
                    }
                    putchar(*ch);
                }
            }
        }
        printf("\",\"trailing_newline\":%s,\"interpret_escapes\":%s}\n",
               trailing_newline ? "true" : "false",
               interpret_escapes ? "true" : "false");
        return ferror(stdout) ? 1 : 0;
    }

    int text_start = index;

    for (; index < argc; index++) {
        if (index > text_start) {
            putchar(' ');
        }
        if (interpret_escapes) {
            const char *ch;

            for (ch = argv[index]; *ch != '\0'; ch++) {
                if (*ch == '\\' && ch[1] != '\0') {
                    ch++;
                    switch (*ch) {
                    case 'n': putchar('\n'); break;
                    case 't': putchar('\t'); break;
                    case 'r': putchar('\r'); break;
                    case 'b': putchar('\b'); break;
                    case '\\': putchar('\\'); break;
                    default:
                        putchar('\\');
                        putchar(*ch);
                        break;
                    }
                } else {
                    putchar(*ch);
                }
            }
        } else {
            fputs(argv[index], stdout);
        }
    }

    if (trailing_newline) {
        putchar('\n');
    }

    return ferror(stdout) ? 1 : 0;
}

void echo_print_usage(FILE *out)
{
    fprintf(out, "Usage: echo [--json] [-n] [-e|-E] [TEXT...]\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Print text arguments separated by spaces.\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
    fprintf(out, "  %-20s %s\n", "-n", "do not print the trailing newline");
    fprintf(out, "  %-20s %s\n", "-e", "interpret backslash escapes");
    fprintf(out, "  %-20s %s\n", "-E", "disable backslash escape interpretation");
}

cmd_spec_t cmd_echo_spec = {
    .name = "echo",
    .summary = "print text to standard output",
    .long_help = "Print text arguments separated by spaces.",
    .run = echo_run,
    .print_usage = echo_print_usage,
};

void register_echo_command(void)
{
    register_command(&cmd_echo_spec);
}
