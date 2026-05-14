#include <stdio.h>
#include <string.h>
#include "cmd_spec.h"
#include "json_utils.h"

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define SHELL_NAME "busybox_shell"
#define SHELL_VERSION "1.0.0"

void register_all_builtin_commands(void);

struct json_command_list_state {
    int first;
};

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

    fprintf(out, "\nCommon features:\n");
    fprintf(out, "  %-20s %s\n", "--version", "show shell version");
    fprintf(out, "  %-20s %s\n", "help --json", "list commands in JSON format");
    fprintf(out, "  %-20s %s\n", "help <command> --json", "show command help metadata as JSON");
}

static void print_common_command_options(FILE *out)
{
    fprintf(out, "\nCommon options:\n");
    fprintf(out, "  %-20s %s\n", "--version", "show command version and exit");
    fprintf(out, "  %-20s %s\n", "--version --json", "show command version as JSON");
    fprintf(out, "  %-20s %s\n", "-h, --help --json", "show command help metadata as JSON");
}

static int has_arg(int argc, char **argv, const char *short_arg, const char *long_arg)
{
    int index;

    for (index = 0; index < argc; index++) {
        if ((short_arg != NULL && strcmp(argv[index], short_arg) == 0) ||
            (long_arg != NULL && strcmp(argv[index], long_arg) == 0)) {
            return 1;
        }
    }

    return 0;
}

static void print_command_help_json(const cmd_spec_t *spec)
{
    printf("{\"name\":");
    json_print_string(stdout, spec->name);
    printf(",\"summary\":");
    json_print_string(stdout, spec->summary != NULL ? spec->summary : "");
    printf(",\"description\":");
    json_print_string(stdout, spec->long_help != NULL ? spec->long_help : "");
    printf(",\"help\":[");
    json_print_string(stdout, "help <command>");
    printf(",");
    json_print_string(stdout, "<command> -h");
    printf(",");
    json_print_string(stdout, "<command> --help");
    printf(",");
    json_print_string(stdout, "<command> --version");
    printf("]}\n");
}

static void print_command_version(const cmd_spec_t *spec, int json)
{
    if (json) {
        printf("{\"name\":");
        json_print_string(stdout, spec->name);
        printf(",\"package\":");
        json_print_string(stdout, SHELL_NAME);
        printf(",\"version\":");
        json_print_string(stdout, SHELL_VERSION);
        printf("}\n");
    } else {
        printf("%s (%s) %s\n", spec->name, SHELL_NAME, SHELL_VERSION);
    }
}

static void print_shell_version(int json)
{
    if (json) {
        printf("{\"name\":");
        json_print_string(stdout, SHELL_NAME);
        printf(",\"version\":");
        json_print_string(stdout, SHELL_VERSION);
        printf("}\n");
    } else {
        printf("%s %s\n", SHELL_NAME, SHELL_VERSION);
    }
}

static void print_command_summary_json(const cmd_spec_t *spec, void *userdata)
{
    struct json_command_list_state *state = userdata;

    if (!state->first) {
        putchar(',');
    }

    printf("{\"name\":");
    json_print_string(stdout, spec->name);
    printf(",\"summary\":");
    json_print_string(stdout, spec->summary != NULL ? spec->summary : "");
    printf("}");

    state->first = 0;
}

static void print_shell_help_json(void)
{
    struct json_command_list_state state = {1};

    printf("{\"builtins\":[");
    printf("{\"name\":\"help\",\"summary\":\"show this help, or help for a command\"},");
    printf("{\"name\":\"exit\",\"summary\":\"exit the shell\"},");
    printf("{\"name\":\"quit\",\"summary\":\"exit the shell\"}");
    printf("],\"commands\":[");
    for_each_command(print_command_summary_json, &state);
    printf("]}\n");
}

static const char *first_non_flag_after_help(int argc, char **argv)
{
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "--json") != 0 &&
            strcmp(argv[index], "-h") != 0 &&
            strcmp(argv[index], "--help") != 0) {
            return argv[index];
        }
    }

    return NULL;
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

    if (strcmp(argv[0], "--version") == 0 || strcmp(argv[0], "version") == 0) {
        print_shell_version(has_arg(argc, argv, NULL, "--json"));
        return 0;
    }

    if (strcmp(argv[0], "help") == 0) {
        int json = has_arg(argc, argv, NULL, "--json");
        const char *command_name = first_non_flag_after_help(argc, argv);

        if (command_name == NULL) {
            if (json) {
                print_shell_help_json();
            } else {
                print_shell_help(stdout);
            }
            return 0;
        }

        cmd = find_command(command_name);
        if (cmd == NULL) {
            fprintf(stderr, "Unknown command: %s\n", command_name);
            return 1;
        }

        if (json) {
            print_command_help_json(cmd);
        } else {
            cmd->print_usage(stdout);
            print_common_command_options(stdout);
        }
        return 0;
    }

    cmd = find_command(argv[0]);

    if (!cmd) {
        fprintf(stderr, "Unknown command: %s\n", argv[0]);
        return 1;
    }

    if (has_arg(argc, argv, NULL, "--version")) {
        print_command_version(cmd, has_arg(argc, argv, NULL, "--json"));
        return 0;
    }

    if (has_arg(argc, argv, "-h", "--help")) {
        if (has_arg(argc, argv, NULL, "--json")) {
            print_command_help_json(cmd);
        } else {
            cmd->print_usage(stdout);
            print_common_command_options(stdout);
        }
        return 0;
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
