#include <stdio.h>
#include <string.h>
#include "cmd_spec.h"

#define MAX_INPUT 1024
#define MAX_ARGS 64

void register_all_builtin_commands(void);

static void print_command_summary(const cmd_spec_t *spec, void *userdata)
{
    FILE *out = userdata;

    fprintf(out, "  %-12s %s\n", spec->name, spec->summary);
}

static void print_shell_help(FILE *out)
{
    fprintf(out, "Built-in shell commands:\n");
    fprintf(out, "  %-12s %s\n", "help", "show this help, or help for a command");
    fprintf(out, "  %-12s %s\n", "exit", "exit the shell");
    fprintf(out, "  %-12s %s\n", "quit", "exit the shell");

    fprintf(out, "\nRegistered commands:\n");
    for_each_command(print_command_summary, out);
}

static int split_line(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *token = strtok(line, " \t\r\n");

    while (token != NULL && argc < max_args - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t\r\n");
    }

    argv[argc] = NULL;
    return argc;
}

static int dispatch_command(int argc, char **argv)
{
    const cmd_spec_t *cmd;

    if (argc == 0) {
        return 0;
    }

    if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0) {
        return -1;
    }

    if (strcmp(argv[0], "help") == 0) {
        if (argc == 1) {
            print_shell_help(stdout);
            return 0;
        }

        cmd = find_command(argv[1]);
        if (cmd == NULL) {
            fprintf(stderr, "Unknown command: %s\n", argv[1]);
            return 1;
        }

        cmd->print_usage(stdout);
        return 0;
    }

    cmd = find_command(argv[0]);

    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n", argv[0]);
        return 1;
    }

    return cmd->run(argc, argv);
}

static int run_interactive_shell(void)
{
    char line[MAX_INPUT];
    char *argv[MAX_ARGS];
    int argc;
    int status = 0;

    while (1) {
        printf("busybox_shell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            return status;
        }

        argc = split_line(line, argv, MAX_ARGS);
        status = dispatch_command(argc, argv);

        if (status < 0) {
            return 0;
        }
    }
}

int main(int argc, char **argv)
{
    int status;

    register_all_builtin_commands();

    if (argc < 2) {
        return run_interactive_shell();
    }

    status = dispatch_command(argc - 1, argv + 1);
    return status < 0 ? 0 : status;
}
