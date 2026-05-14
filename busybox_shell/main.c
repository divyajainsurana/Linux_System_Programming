#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "cmd_spec.h"
#include "json_utils.h"

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_COMPLETIONS 128
#define SHELL_NAME "busybox_shell"
#define SHELL_VERSION "1.0.0"
#define HISTORY_FILE ".busybox_shell_history"

static char *history[MAX_HISTORY];
static int history_count;

struct completion_list {
    char *items[MAX_COMPLETIONS];
    int count;
};

struct command_completion_state {
    const char *prefix;
    size_t prefix_length;
    struct completion_list *list;
};

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

static char *shell_strdup(const char *text)
{
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, text, length);
    }

    return copy;
}

static int is_blank_line(const char *line)
{
    while (*line != '\0') {
        if (*line != ' ' && *line != '\t' && *line != '\r' && *line != '\n') {
            return 0;
        }
        line++;
    }

    return 1;
}

static void trim_newline(char *line)
{
    size_t length = strlen(line);

    while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r')) {
        line[length - 1] = '\0';
        length--;
    }
}

static void add_history_entry(const char *line)
{
    char *copy;
    int index;

    if (line == NULL || is_blank_line(line)) {
        return;
    }

    if (history_count > 0 && strcmp(history[history_count - 1], line) == 0) {
        return;
    }

    copy = shell_strdup(line);
    if (copy == NULL) {
        return;
    }

    if (history_count == MAX_HISTORY) {
        free(history[0]);
        for (index = 1; index < MAX_HISTORY; index++) {
            history[index - 1] = history[index];
        }
        history_count--;
    }

    history[history_count++] = copy;
}

static void get_history_path(char *path, size_t path_size)
{
    const char *home = getenv("HOME");

    if (home != NULL && *home != '\0') {
        snprintf(path, path_size, "%s/%s", home, HISTORY_FILE);
    } else {
        snprintf(path, path_size, "%s", HISTORY_FILE);
    }
}

static void load_history(void)
{
    char path[MAX_INPUT];
    char line[MAX_INPUT];
    FILE *in;

    get_history_path(path, sizeof(path));
    in = fopen(path, "r");
    if (in == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), in) != NULL) {
        trim_newline(line);
        add_history_entry(line);
    }

    fclose(in);
}

static void save_history(void)
{
    char path[MAX_INPUT];
    FILE *out;
    int index;

    get_history_path(path, sizeof(path));
    out = fopen(path, "w");
    if (out == NULL) {
        return;
    }

    for (index = 0; index < history_count; index++) {
        fprintf(out, "%s\n", history[index]);
    }

    fclose(out);
}

static void free_history(void)
{
    int index;

    for (index = 0; index < history_count; index++) {
        free(history[index]);
    }
    history_count = 0;
}

static void free_completions(struct completion_list *list)
{
    int index;

    for (index = 0; index < list->count; index++) {
        free(list->items[index]);
    }
    list->count = 0;
}

static int completion_exists(const struct completion_list *list, const char *item)
{
    int index;

    for (index = 0; index < list->count; index++) {
        if (strcmp(list->items[index], item) == 0) {
            return 1;
        }
    }

    return 0;
}

static void add_completion(struct completion_list *list, const char *item)
{
    char *copy;

    if (list->count >= MAX_COMPLETIONS || completion_exists(list, item)) {
        return;
    }

    copy = shell_strdup(item);
    if (copy == NULL) {
        return;
    }

    list->items[list->count++] = copy;
}

static void add_command_completion(const char *name, struct completion_list *list,
                                   const char *prefix, size_t prefix_length)
{
    if (strncmp(name, prefix, prefix_length) == 0) {
        add_completion(list, name);
    }
}

static void collect_registered_command_completion(const cmd_spec_t *spec, void *userdata)
{
    struct command_completion_state *state = userdata;

    add_command_completion(spec->name, state->list, state->prefix, state->prefix_length);
}

static void collect_command_completions(const char *prefix, struct completion_list *list)
{
    static const char *builtins[] = {"help", "exit", "quit", "version"};
    struct command_completion_state state;
    size_t prefix_length = strlen(prefix);
    size_t index;

    for (index = 0; index < sizeof(builtins) / sizeof(builtins[0]); index++) {
        add_command_completion(builtins[index], list, prefix, prefix_length);
    }

    state.prefix = prefix;
    state.prefix_length = prefix_length;
    state.list = list;
    for_each_command(collect_registered_command_completion, &state);
}

static int path_is_directory(const char *path)
{
    struct stat info;

    return stat(path, &info) == 0 && S_ISDIR(info.st_mode);
}

static void join_path(char *out, size_t out_size, const char *dir, const char *name)
{
    if (strcmp(dir, ".") == 0) {
        snprintf(out, out_size, "%s", name);
    } else if (dir[0] != '\0' && dir[strlen(dir) - 1] == '/') {
        snprintf(out, out_size, "%s%s", dir, name);
    } else {
        snprintf(out, out_size, "%s/%s", dir, name);
    }
}

static void collect_path_completions(const char *word, struct completion_list *list)
{
    char directory[MAX_INPUT];
    char base[MAX_INPUT];
    char display_prefix[MAX_INPUT];
    const char *slash = strrchr(word, '/');
    DIR *dir;
    struct dirent *entry;

    if (slash != NULL) {
        size_t directory_length = (size_t) (slash - word);
        size_t prefix_length = (size_t) (slash - word) + 1;

        if (directory_length == 0) {
            snprintf(directory, sizeof(directory), "/");
        } else {
            snprintf(directory, sizeof(directory), "%.*s", (int) directory_length, word);
        }
        snprintf(display_prefix, sizeof(display_prefix), "%.*s", (int) prefix_length, word);
        snprintf(base, sizeof(base), "%s", slash + 1);
    } else {
        snprintf(directory, sizeof(directory), ".");
        display_prefix[0] = '\0';
        snprintf(base, sizeof(base), "%s", word);
    }

    dir = opendir(directory);
    if (dir == NULL) {
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[MAX_INPUT];
        char completion[MAX_INPUT];
        size_t base_length = strlen(base);

        if (base[0] != '.' && entry->d_name[0] == '.') {
            continue;
        }

        if (strncmp(entry->d_name, base, base_length) != 0) {
            continue;
        }

        join_path(full_path, sizeof(full_path), directory, entry->d_name);
        snprintf(completion, sizeof(completion), "%s%s%s",
                 display_prefix,
                 entry->d_name,
                 path_is_directory(full_path) ? "/" : "");
        add_completion(list, completion);
    }

    closedir(dir);
}

static size_t current_word_start(const char *line, size_t cursor)
{
    size_t start = cursor;

    while (start > 0 && line[start - 1] != ' ' && line[start - 1] != '\t') {
        start--;
    }

    return start;
}

static int completing_command_word(const char *line, size_t word_start)
{
    size_t index;

    for (index = 0; index < word_start; index++) {
        if (line[index] != ' ' && line[index] != '\t') {
            return 0;
        }
    }

    return 1;
}

static void longest_common_prefix(const struct completion_list *list, char *out, size_t out_size)
{
    size_t prefix_length;
    int index;

    if (list->count == 0) {
        out[0] = '\0';
        return;
    }

    snprintf(out, out_size, "%s", list->items[0]);
    prefix_length = strlen(out);

    for (index = 1; index < list->count; index++) {
        size_t current = 0;

        while (current < prefix_length &&
               list->items[index][current] != '\0' &&
               out[current] == list->items[index][current]) {
            current++;
        }

        prefix_length = current;
        out[prefix_length] = '\0';
    }
}

static int replace_current_word(char *line, size_t line_size, size_t *length,
                                size_t *cursor, size_t word_start,
                                const char *replacement)
{
    size_t replacement_length = strlen(replacement);
    size_t suffix_length = *length - *cursor;
    size_t new_length = word_start + replacement_length + suffix_length;

    if (new_length >= line_size) {
        return 0;
    }

    memmove(line + word_start + replacement_length,
            line + *cursor,
            suffix_length + 1);
    memcpy(line + word_start, replacement, replacement_length);
    *cursor = word_start + replacement_length;
    *length = new_length;
    return 1;
}

static void print_completion_matches(const struct completion_list *list)
{
    int index;

    putchar('\n');
    for (index = 0; index < list->count; index++) {
        printf("%s", list->items[index]);
        if (index + 1 < list->count) {
            printf("  ");
        }
    }
    putchar('\n');
}

static void refresh_input_line(const char *prompt, const char *line, size_t cursor)
{
    size_t length = strlen(line);

    printf("\r%s%s\033[K", prompt, line);
    if (length > cursor) {
        printf("\033[%zuD", length - cursor);
    }
    fflush(stdout);
}

static void complete_current_word(const char *prompt, char *line, size_t line_size,
                                  size_t *length, size_t *cursor)
{
    struct completion_list list = {{0}, 0};
    char word[MAX_INPUT];
    char common[MAX_INPUT];
    size_t word_start = current_word_start(line, *cursor);
    size_t word_length = *cursor - word_start;

    snprintf(word, sizeof(word), "%.*s", (int) word_length, line + word_start);

    if (completing_command_word(line, word_start)) {
        collect_command_completions(word, &list);
    } else {
        collect_path_completions(word, &list);
    }

    if (list.count == 1) {
        char replacement[MAX_INPUT];
        const char *match = list.items[0];
        size_t match_length = strlen(match);

        snprintf(replacement, sizeof(replacement), "%s%s",
                 match,
                 match_length > 0 && match[match_length - 1] != '/' ? " " : "");
        replace_current_word(line, line_size, length, cursor, word_start, replacement);
        refresh_input_line(prompt, line, *cursor);
    } else if (list.count > 1) {
        longest_common_prefix(&list, common, sizeof(common));
        if (strlen(common) > word_length) {
            replace_current_word(line, line_size, length, cursor, word_start, common);
        } else {
            print_completion_matches(&list);
        }
        refresh_input_line(prompt, line, *cursor);
    }

    free_completions(&list);
}

static int read_line_raw(const char *prompt, char *line, size_t line_size)
{
    struct termios original;
    struct termios raw;
    char saved_line[MAX_INPUT] = "";
    size_t length = 0;
    size_t cursor = 0;
    int history_index = history_count;

    if (tcgetattr(STDIN_FILENO, &original) != 0) {
        return 0;
    }

    raw = original;
    raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        return 0;
    }

    line[0] = '\0';
    printf("%s", prompt);
    fflush(stdout);

    while (1) {
        unsigned char ch;

        if (read(STDIN_FILENO, &ch, 1) != 1) {
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
            return 0;
        }

        if (ch == '\n' || ch == '\r') {
            putchar('\n');
            line[length] = '\0';
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
            return 1;
        }

        if (ch == 4 && length == 0) {
            putchar('\n');
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
            return 0;
        }

        if (ch == 127 || ch == 8) {
            if (cursor > 0) {
                memmove(line + cursor - 1, line + cursor, length - cursor + 1);
                cursor--;
                length--;
                refresh_input_line(prompt, line, cursor);
            }
            continue;
        }

        if (ch == '\t') {
            complete_current_word(prompt, line, line_size, &length, &cursor);
            continue;
        }

        if (ch == 27) {
            unsigned char seq[2];

            if (read(STDIN_FILENO, &seq[0], 1) != 1 ||
                read(STDIN_FILENO, &seq[1], 1) != 1) {
                continue;
            }

            if (seq[0] != '[') {
                continue;
            }

            if (seq[1] == 'A' && history_count > 0) {
                if (history_index == history_count) {
                    snprintf(saved_line, sizeof(saved_line), "%s", line);
                }
                if (history_index > 0) {
                    history_index--;
                    snprintf(line, line_size, "%s", history[history_index]);
                    length = strlen(line);
                    cursor = length;
                    refresh_input_line(prompt, line, cursor);
                }
            } else if (seq[1] == 'B') {
                if (history_index < history_count - 1) {
                    history_index++;
                    snprintf(line, line_size, "%s", history[history_index]);
                } else if (history_index < history_count) {
                    history_index = history_count;
                    snprintf(line, line_size, "%s", saved_line);
                }
                length = strlen(line);
                cursor = length;
                refresh_input_line(prompt, line, cursor);
            } else if (seq[1] == 'C') {
                if (cursor < length) {
                    cursor++;
                    printf("\033[C");
                    fflush(stdout);
                }
            } else if (seq[1] == 'D') {
                if (cursor > 0) {
                    cursor--;
                    printf("\033[D");
                    fflush(stdout);
                }
            }
            continue;
        }

        if (ch >= 32 && ch <= 126 && length < line_size - 1) {
            memmove(line + cursor + 1, line + cursor, length - cursor + 1);
            line[cursor] = (char) ch;
            cursor++;
            length++;
            refresh_input_line(prompt, line, cursor);
        }
    }
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
    int interactive_terminal = isatty(STDIN_FILENO);

    if (interactive_terminal) {
        load_history();
    }

    while (1) {
        if (interactive_terminal) {
            if (!read_line_raw("busybox_shell> ", line, sizeof(line))) {
                save_history();
                free_history();
                return status;
            }
        } else {
            printf("busybox_shell> ");
            fflush(stdout);

            if (fgets(line, sizeof(line), stdin) == NULL) {
                printf("\n");
                return status;
            }
            trim_newline(line);
        }

        if (interactive_terminal) {
            add_history_entry(line);
        }
        argc = split_line(line, argv, MAX_ARGS);
        status = dispatch_command(argc, argv);

        if (status < 0) {
            if (interactive_terminal) {
                save_history();
                free_history();
            }
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
