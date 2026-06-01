#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "cmd_spec.h"
#include "json_utils.h"
#include "bnfc/Absyn.h"
#include "bnfc/Parser.h"

#define MAX_INPUT 1024
#define MAX_ARGS 64
#define MAX_HISTORY 100
#define MAX_COMPLETIONS 128
#define MAX_NL_CACHE 64
#define MAX_PIPE_COMMANDS 16
#define MAX_SHELL_VARIABLES 128
#define SHELL_NAME "busybox_shell"
#define SHELL_VERSION "1.0.0"
#define HISTORY_FILE ".busybox_shell_history"
#define INTERNAL_RUN_COMMAND_FLAG "__busybox_shell_run_command"

static const char *shell_program_path;
static char *history[MAX_HISTORY];
static int history_count;

struct nl_cache_entry {
    char request[MAX_INPUT];
    char command[MAX_INPUT];
};

static struct nl_cache_entry nl_cache[MAX_NL_CACHE];
static int nl_cache_count;

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

static int execute_pipeline(int command_count, int *argc_list, char ***argv_list);
static int run_exec_child(int argc, char **argv);
static int execute_command_process(int argc, char **argv,
                                   const char *input_file,
                                   const char *output_file);
static int bnfc_dispatch_command_line(CommandLine commandline);
static int bnfc_dispatch_job(Job job);

struct shell_variable {
    char *name;
    char *value;
};

static struct shell_variable shell_variables[MAX_SHELL_VARIABLES];
static int shell_variable_count;

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
    fprintf(out, "  %-20s %s\n", "@ <request>", "ask for an AI command suggestion");
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

static const char *skip_spaces(const char *text)
{
    while (*text == ' ' || *text == '\t') {
        text++;
    }

    return text;
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

static int is_valid_variable_name(const char *name)
{
    size_t index;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }

    if (!((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z') ||
          name[0] == '_')) {
        return 0;
    }

    for (index = 1; name[index] != '\0'; index++) {
        if (!((name[index] >= 'A' && name[index] <= 'Z') ||
              (name[index] >= 'a' && name[index] <= 'z') ||
              (name[index] >= '0' && name[index] <= '9') ||
              name[index] == '_')) {
            return 0;
        }
    }

    return 1;
}

static const char *get_shell_variable(const char *name)
{
    int index;

    for (index = 0; index < shell_variable_count; index++) {
        if (strcmp(shell_variables[index].name, name) == 0) {
            return shell_variables[index].value;
        }
    }

    return "";
}

static int set_shell_variable(const char *name, const char *value)
{
    int index;
    char *name_copy;
    char *value_copy;

    if (!is_valid_variable_name(name)) {
        fprintf(stderr, "Invalid variable name: %s\n", name != NULL ? name : "");
        return 1;
    }

    value_copy = shell_strdup(value != NULL ? value : "");
    if (value_copy == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    for (index = 0; index < shell_variable_count; index++) {
        if (strcmp(shell_variables[index].name, name) == 0) {
            free(shell_variables[index].value);
            shell_variables[index].value = value_copy;
            return 0;
        }
    }

    if (shell_variable_count >= MAX_SHELL_VARIABLES) {
        free(value_copy);
        fprintf(stderr, "Too many shell variables\n");
        return 1;
    }

    name_copy = shell_strdup(name);
    if (name_copy == NULL) {
        free(value_copy);
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    shell_variables[shell_variable_count].name = name_copy;
    shell_variables[shell_variable_count].value = value_copy;
    shell_variable_count++;
    return 0;
}

static void free_shell_variables(void)
{
    int index;

    for (index = 0; index < shell_variable_count; index++) {
        free(shell_variables[index].name);
        free(shell_variables[index].value);
        shell_variables[index].name = NULL;
        shell_variables[index].value = NULL;
    }

    shell_variable_count = 0;
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

static void normalize_nl_request(const char *request, char *out, size_t out_size)
{
    size_t read_index = 0;
    size_t write_index = 0;
    int previous_space = 1;

    while (request[read_index] != '\0' && write_index + 1 < out_size) {
        char ch = request[read_index];

        if (ch >= 'A' && ch <= 'Z') {
            ch = (char) (ch - 'A' + 'a');
        }

        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
            if (!previous_space) {
                out[write_index++] = ' ';
                previous_space = 1;
            }
        } else {
            out[write_index++] = ch;
            previous_space = 0;
        }
        read_index++;
    }

    if (write_index > 0 && out[write_index - 1] == ' ') {
        write_index--;
    }
    out[write_index] = '\0';
}

static int lookup_nl_cache(const char *request, char *command, size_t command_size)
{
    char normalized[MAX_INPUT];
    int index;

    normalize_nl_request(request, normalized, sizeof(normalized));
    for (index = 0; index < nl_cache_count; index++) {
        if (strcmp(nl_cache[index].request, normalized) == 0) {
            snprintf(command, command_size, "%s", nl_cache[index].command);
            return 1;
        }
    }

    return 0;
}

static void store_nl_cache(const char *request, const char *command)
{
    char normalized[MAX_INPUT];
    int index;

    normalize_nl_request(request, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
        return;
    }

    for (index = 0; index < nl_cache_count; index++) {
        if (strcmp(nl_cache[index].request, normalized) == 0) {
            snprintf(nl_cache[index].command, sizeof(nl_cache[index].command), "%s", command);
            return;
        }
    }

    if (nl_cache_count == MAX_NL_CACHE) {
        for (index = 1; index < MAX_NL_CACHE; index++) {
            nl_cache[index - 1] = nl_cache[index];
        }
        nl_cache_count--;
    }

    snprintf(nl_cache[nl_cache_count].request, sizeof(nl_cache[nl_cache_count].request),
             "%s", normalized);
    snprintf(nl_cache[nl_cache_count].command, sizeof(nl_cache[nl_cache_count].command),
             "%s", command);
    nl_cache_count++;
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
        execvp(argv[0], argv);
        fprintf(stderr, "%s: %s\n", argv[0], strerror(errno));
        return 127;
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

static void bnfc_collect_redirections(ListRedirection list,
                                      const char **input_file,
                                      const char **output_file)
{
    while (list != NULL) {
        Redirection redir = list->redirection_;

        if (redir != NULL) {
            switch (redir->kind) {
            case is_InputRedirection:
                *input_file = redir->u.inputRedirection_.word_;
                break;
            case is_OutputRedirection:
                *output_file = redir->u.outputRedirection_.word_;
                break;
            }
        }

        list = list->listredirection_;
    }
}

static void trim_command_substitution_output(char *text)
{
    size_t length;
    size_t index;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[length - 1] = '\0';
        length--;
    }

    for (index = 0; text[index] != '\0'; index++) {
        if (text[index] == '\n' || text[index] == '\r' || text[index] == '\t') {
            text[index] = ' ';
        }
    }
}

static char *bnfc_capture_subcommand(Word command, ListWord words)
{
    int pipefd[2];
    pid_t pid;
    char buffer[MAX_INPUT];
    size_t used = 0;

    if (pipe(pipefd) < 0) {
        fprintf(stderr, "pipe: %s\n", strerror(errno));
        return NULL;
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return NULL;
    }

    if (pid == 0) {
        char *argv[MAX_ARGS];
        int argc = 0;

        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
            fprintf(stderr, "dup2: %s\n", strerror(errno));
            _exit(1);
        }
        close(pipefd[1]);

        argv[argc++] = command;
        while (words != NULL && argc < MAX_ARGS - 1) {
            argv[argc++] = words->word_;
            words = words->listword_;
        }
        argv[argc] = NULL;

        run_exec_child(argc, argv);
    }

    close(pipefd[1]);
    while (used + 1 < sizeof(buffer)) {
        ssize_t count = read(pipefd[0], buffer + used, sizeof(buffer) - used - 1);

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "read: %s\n", strerror(errno));
            break;
        }
        if (count == 0) {
            break;
        }
        used += (size_t) count;
    }
    close(pipefd[0]);

    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }

    buffer[used] = '\0';
    trim_command_substitution_output(buffer);
    return shell_strdup(buffer);
}

static char *bnfc_expand_arg(Arg arg)
{
    if (arg == NULL) {
        return shell_strdup("");
    }

    switch (arg->kind) {
    case is_WordArg:
        return shell_strdup(arg->u.wordArg_.word_);

    case is_VariableArg:
        return shell_strdup(get_shell_variable(arg->u.variableArg_.word_));

    case is_SubcommandArg:
        return bnfc_capture_subcommand(arg->u.subcommandArg_.word_,
                                       arg->u.subcommandArg_.listword_);
    }

    return NULL;
}

static void bnfc_free_argv(int argc, char **argv)
{
    int index;

    for (index = 0; index < argc; index++) {
        free(argv[index]);
        argv[index] = NULL;
    }
}

static void bnfc_free_pipeline_argvs(int command_count, int *argc_list, char ***argv_list)
{
    int command_index;

    for (command_index = 0; command_index < command_count; command_index++) {
        bnfc_free_argv(argc_list[command_index], argv_list[command_index]);
    }
}

static int bnfc_build_argv(CommandPart part, char **argv, int max_args)
{
    ListArg args;
    int argc = 0;

    if (part == NULL || part->kind != is_MkCommandPart || max_args <= 1) {
        return 0;
    }

    argv[argc] = shell_strdup(part->u.mkCommandPart_.word_);
    if (argv[argc] == NULL) {
        fprintf(stderr, "Out of memory\n");
        return 0;
    }
    argc++;

    args = part->u.mkCommandPart_.listarg_;

    while (args != NULL && argc < max_args - 1) {
        argv[argc] = bnfc_expand_arg(args->arg_);
        if (argv[argc] == NULL) {
            fprintf(stderr, "Failed to expand argument\n");
            bnfc_free_argv(argc, argv);
            return 0;
        }
        argc++;
        args = args->listarg_;
    }

    argv[argc] = NULL;
    return argc;
}

static int bnfc_collect_pipeline(Pipeline pipeline,
                                 int *argc_list,
                                 char ***argv_list,
                                 char *argv_storage[MAX_PIPE_COMMANDS][MAX_ARGS],
                                 int *command_count)
{
    CommandPart part;

    if (pipeline == NULL || *command_count >= MAX_PIPE_COMMANDS) {
        return 1;
    }

    switch (pipeline->kind) {
    case is_SingleCommand:
        part = pipeline->u.singleCommand_.commandpart_;
        argv_list[*command_count] = argv_storage[*command_count];
        argc_list[*command_count] = bnfc_build_argv(part, argv_storage[*command_count], MAX_ARGS);
        if (argc_list[*command_count] == 0) {
            fprintf(stderr, "Invalid empty command in pipeline\n");
            return 1;
        }
        (*command_count)++;
        return 0;

    case is_PipeCommand:
        part = pipeline->u.pipeCommand_.commandpart_;
        argv_list[*command_count] = argv_storage[*command_count];
        argc_list[*command_count] = bnfc_build_argv(part, argv_storage[*command_count], MAX_ARGS);
        if (argc_list[*command_count] == 0) {
            fprintf(stderr, "Invalid empty command in pipeline\n");
            return 1;
        }
        (*command_count)++;
        return bnfc_collect_pipeline(pipeline->u.pipeCommand_.pipeline_,
                                     argc_list, argv_list, argv_storage, command_count);
    }

    return 1;
}

static int bnfc_redirect_fd(const char *path, int target_fd, int flags, mode_t mode)
{
    int fd = open(path, flags, mode);

    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        return 1;
    }

    if (dup2(fd, target_fd) < 0) {
        fprintf(stderr, "dup2: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

static int bnfc_dispatch_simple_with_redirection(int argc,
                                                char **argv,
                                                const char *input_file,
                                                const char *output_file)
{
    if (argc > 0 &&
        (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0)) {
        return dispatch_command(argc, argv);
    }

    return execute_command_process(argc, argv, input_file, output_file);
}

static int bnfc_execute_pipeline_with_redirection(int command_count,
                                                 int *argc_list,
                                                 char ***argv_list,
                                                 const char *input_file,
                                                 const char *output_file)
{
    int pipes[MAX_PIPE_COMMANDS - 1][2];
    pid_t pids[MAX_PIPE_COMMANDS];
    int created_pipes = 0;
    int started_children = 0;
    int command_index;
    int last_status = 0;

    if (command_count <= 0) {
        return 0;
    }

    if (shell_program_path == NULL || shell_program_path[0] == '\0') {
        shell_program_path = "./busybox_shell";
    }

    for (command_index = 0; command_index < command_count - 1; command_index++) {
        if (pipe(pipes[command_index]) < 0) {
            fprintf(stderr, "pipe: %s\n", strerror(errno));
            return 1;
        }
        created_pipes++;
    }

    for (command_index = 0; command_index < command_count; command_index++) {
        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "fork: %s\n", strerror(errno));
            last_status = 1;
            break;
        }

        if (pid == 0) {
            int pipe_index;

            if (command_index == 0 && input_file != NULL &&
                bnfc_redirect_fd(input_file, STDIN_FILENO, O_RDONLY, 0) != 0) {
                _exit(1);
            }

            if (command_index == command_count - 1 && output_file != NULL &&
                bnfc_redirect_fd(output_file, STDOUT_FILENO,
                                 O_WRONLY | O_CREAT | O_TRUNC, 0644) != 0) {
                _exit(1);
            }

            if (command_index > 0 &&
                dup2(pipes[command_index - 1][0], STDIN_FILENO) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                _exit(1);
            }

            if (command_index < command_count - 1 &&
                dup2(pipes[command_index][1], STDOUT_FILENO) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                _exit(1);
            }

            for (pipe_index = 0; pipe_index < created_pipes; pipe_index++) {
                close(pipes[pipe_index][0]);
                close(pipes[pipe_index][1]);
            }

            run_exec_child(argc_list[command_index], argv_list[command_index]);
        }

        pids[started_children++] = pid;
    }

    for (command_index = 0; command_index < created_pipes; command_index++) {
        close(pipes[command_index][0]);
        close(pipes[command_index][1]);
    }

    for (command_index = 0; command_index < started_children; command_index++) {
        int status;

        if (waitpid(pids[command_index], &status, 0) < 0) {
            fprintf(stderr, "waitpid: %s\n", strerror(errno));
            last_status = 1;
            continue;
        }

        if (command_index == started_children - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            } else {
                last_status = 1;
            }
        }
    }

    return last_status;
}

static int bnfc_dispatch_command_line(CommandLine commandline)
{
    int argc_list[MAX_PIPE_COMMANDS];
    char **argv_list[MAX_PIPE_COMMANDS];
    char *argv_storage[MAX_PIPE_COMMANDS][MAX_ARGS];
    int command_count = 0;
    Pipeline pipeline;
    const char *input_file = NULL;
    const char *output_file = NULL;
    int status;

    if (commandline == NULL || commandline->kind != is_CommandWithRedir) {
        return 1;
    }

    bnfc_collect_redirections(commandline->u.commandWithRedir_.listredirection_,
                              &input_file, &output_file);

    pipeline = commandline->u.commandWithRedir_.pipeline_;
    if (bnfc_collect_pipeline(pipeline, argc_list, argv_list, argv_storage, &command_count) != 0) {
        bnfc_free_pipeline_argvs(command_count, argc_list, argv_list);
        return 1;
    }

    if (command_count == 1) {
        status = bnfc_dispatch_simple_with_redirection(argc_list[0], argv_list[0],
                                                       input_file, output_file);
        bnfc_free_pipeline_argvs(command_count, argc_list, argv_list);
        return status;
    }

    if (input_file != NULL || output_file != NULL) {
        status = bnfc_execute_pipeline_with_redirection(command_count, argc_list,
                                                        argv_list, input_file, output_file);
        bnfc_free_pipeline_argvs(command_count, argc_list, argv_list);
        return status;
    }

    status = execute_pipeline(command_count, argc_list, argv_list);
    bnfc_free_pipeline_argvs(command_count, argc_list, argv_list);
    return status;
}

static int bnfc_dispatch_assignment(Assignment assignment)
{
    char *value;
    int status;

    if (assignment == NULL || assignment->kind != is_SetVariable) {
        return 1;
    }

    value = bnfc_expand_arg(assignment->u.setVariable_.arg_);
    if (value == NULL) {
        fprintf(stderr, "Failed to expand variable value\n");
        return 1;
    }

    status = set_shell_variable(assignment->u.setVariable_.word_, value);
    free(value);
    return status;
}

static int bnfc_dispatch_job_list(ListJob jobs)
{
    int status = 0;

    while (jobs != NULL) {
        status = bnfc_dispatch_job(jobs->job_);
        if (status < 0) {
            return status;
        }
        jobs = jobs->listjob_;
    }

    return status;
}

static int bnfc_dispatch_if_statement(IfStatement ifstatement)
{
    int status;

    if (ifstatement == NULL || ifstatement->kind != is_IfThenFi) {
        return 1;
    }

    status = bnfc_dispatch_command_line(ifstatement->u.ifThenFi_.commandline_);
    if (status == 0) {
        return bnfc_dispatch_job_list(ifstatement->u.ifThenFi_.listjob_);
    }

    return status;
}

static int bnfc_dispatch_job(Job job)
{
    CommandLine commandline;

    if (job == NULL) {
        return 0;
    }

    switch (job->kind) {
    case is_ForegroundJob:
        return bnfc_dispatch_command_line(job->u.foregroundJob_.commandline_);

    case is_BackgroundJob:
        commandline = job->u.backgroundJob_.commandline_;
        fprintf(stderr, "Background jobs parsed, but BNFC execution does not handle them yet\n");
        (void) commandline;
        return 1;

    case is_AssignmentJob:
        return bnfc_dispatch_assignment(job->u.assignmentJob_.assignment_);

    case is_IfJob:
        return bnfc_dispatch_if_statement(job->u.ifJob_.ifstatement_);
    }

    return 1;
}

static int dispatch_bnfc_line(const char *line)
{
    Input input;
    ListJob jobs;
    int status = 0;

    input = psInput(line);
    if (input == NULL || input->kind != is_StartInput) {
        return 2;
    }

    jobs = input->u.startInput_.listjob_;
    while (jobs != NULL) {
        status = bnfc_dispatch_job(jobs->job_);
        if (status < 0) {
            break;
        }
        jobs = jobs->listjob_;
    }

    free_Input(input);
    return status;
}

static int line_contains_shell_syntax(const char *line)
{
    return strpbrk(line, "|<>&;") != NULL;
}

static int run_exec_child(int argc, char **argv)
{
    char *child_argv[MAX_ARGS + 3];
    int index;

    child_argv[0] = (char *) shell_program_path;
    child_argv[1] = INTERNAL_RUN_COMMAND_FLAG;
    for (index = 0; index < argc && index + 3 < MAX_ARGS + 3; index++) {
        child_argv[index + 2] = argv[index];
    }
    child_argv[index + 2] = NULL;

    execvp(child_argv[0], child_argv);
    fprintf(stderr, "%s: execvp: %s\n", child_argv[0], strerror(errno));
    _exit(127);
}

static int execute_command_process(int argc, char **argv,
                                   const char *input_file,
                                   const char *output_file)
{
    pid_t pid;
    int status;

    if (argc <= 0) {
        return 0;
    }

    if (shell_program_path == NULL || shell_program_path[0] == '\0') {
        shell_program_path = "./busybox_shell";
    }

    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "fork: %s\n", strerror(errno));
        return 1;
    }

    if (pid == 0) {
        if (input_file != NULL &&
            bnfc_redirect_fd(input_file, STDIN_FILENO, O_RDONLY, 0) != 0) {
            _exit(1);
        }

        if (output_file != NULL &&
            bnfc_redirect_fd(output_file, STDOUT_FILENO,
                             O_WRONLY | O_CREAT | O_TRUNC, 0644) != 0) {
            _exit(1);
        }

        run_exec_child(argc, argv);
    }

    if (waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "waitpid: %s\n", strerror(errno));
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 1;
}

static int execute_pipeline(int command_count, int *argc_list, char ***argv_list)
{
    int pipes[MAX_PIPE_COMMANDS - 1][2];
    pid_t pids[MAX_PIPE_COMMANDS];
    int created_pipes = 0;
    int started_children = 0;
    int command_index;
    int last_status = 0;

    if (command_count <= 0) {
        return 0;
    }

    if (shell_program_path == NULL || shell_program_path[0] == '\0') {
        shell_program_path = "./busybox_shell";
    }

    for (command_index = 0; command_index < command_count - 1; command_index++) {
        if (pipe(pipes[command_index]) < 0) {
            fprintf(stderr, "pipe: %s\n", strerror(errno));
            return 1;
        }
        created_pipes++;
    }

    for (command_index = 0; command_index < command_count; command_index++) {
        pid_t pid = fork();

        if (pid < 0) {
            fprintf(stderr, "fork: %s\n", strerror(errno));
            last_status = 1;
            break;
        }

        if (pid == 0) {
            int pipe_index;

            if (command_index > 0 &&
                dup2(pipes[command_index - 1][0], STDIN_FILENO) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                _exit(1);
            }

            if (command_index < command_count - 1 &&
                dup2(pipes[command_index][1], STDOUT_FILENO) < 0) {
                fprintf(stderr, "dup2: %s\n", strerror(errno));
                _exit(1);
            }

            for (pipe_index = 0; pipe_index < created_pipes; pipe_index++) {
                close(pipes[pipe_index][0]);
                close(pipes[pipe_index][1]);
            }

            run_exec_child(argc_list[command_index], argv_list[command_index]);
        }

        pids[started_children++] = pid;
    }

    for (command_index = 0; command_index < created_pipes; command_index++) {
        close(pipes[command_index][0]);
        close(pipes[command_index][1]);
    }

    for (command_index = 0; command_index < started_children; command_index++) {
        int status;

        if (waitpid(pids[command_index], &status, 0) < 0) {
            fprintf(stderr, "waitpid: %s\n", strerror(errno));
            last_status = 1;
            continue;
        }

        if (command_index == started_children - 1) {
            if (WIFEXITED(status)) {
                last_status = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                last_status = 128 + WTERMSIG(status);
            } else {
                last_status = 1;
            }
        }
    }

    return last_status;
}

static void lowercase_copy(char *out, size_t out_size, const char *in)
{
    size_t index = 0;

    while (in[index] != '\0' && index + 1 < out_size) {
        char ch = in[index];

        if (ch >= 'A' && ch <= 'Z') {
            ch = (char) (ch - 'A' + 'a');
        }
        out[index] = ch;
        index++;
    }
    out[index] = '\0';
}

static int text_contains(const char *text, const char *needle)
{
    return strstr(text, needle) != NULL;
}

static int is_name_char(char ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-' || ch == '.' || ch == '/';
}

static void sanitize_name(char *text)
{
    size_t read_index = 0;
    size_t write_index = 0;

    while (text[read_index] != '\0') {
        if (is_name_char(text[read_index])) {
            text[write_index++] = text[read_index];
        }
        read_index++;
    }

    text[write_index] = '\0';
}

static void sanitize_echo_text(char *text)
{
    size_t read_index = 0;
    size_t write_index = 0;

    while (text[read_index] != '\0') {
        char ch = text[read_index];

        if (ch == '"' || ch == '\'' || ch == ';' || ch == '&' ||
            ch == '|' || ch == '<' || ch == '>' || ch == '`') {
            read_index++;
            continue;
        }

        if (ch == '\t' || ch == '\r' || ch == '\n') {
            ch = ' ';
        }

        text[write_index++] = ch;
        read_index++;
    }

    while (write_index > 0 && text[write_index - 1] == ' ') {
        write_index--;
    }
    text[write_index] = '\0';
}

static int extract_word_after_phrase(const char *text, const char *phrase,
                                     char *out, size_t out_size)
{
    const char *start = strstr(text, phrase);
    size_t index = 0;

    if (start == NULL) {
        return 0;
    }

    start += strlen(phrase);
    start = skip_spaces(start);

    while (start[index] != '\0' &&
           start[index] != ' ' &&
           start[index] != '\t' &&
           index + 1 < out_size) {
        out[index] = start[index];
        index++;
    }
    out[index] = '\0';
    sanitize_name(out);

    return out[0] != '\0';
}

static void command_with_optional_name(char *command, size_t command_size,
                                       const char *base_command,
                                       const char *request,
                                       const char *default_name)
{
    char name[MAX_INPUT];

    if (!extract_word_after_phrase(request, "named", name, sizeof(name)) &&
        !extract_word_after_phrase(request, "called", name, sizeof(name)) &&
        !extract_word_after_phrase(request, "directory", name, sizeof(name)) &&
        !extract_word_after_phrase(request, "folder", name, sizeof(name))) {
        snprintf(name, sizeof(name), "%s", default_name);
    }

    snprintf(command, command_size, "%s %s", base_command, name);
}

static int request_is_create_directory(const char *request)
{
    char lower[MAX_INPUT];

    lowercase_copy(lower, sizeof(lower), request);
    return (text_contains(lower, "create") ||
            text_contains(lower, "make") ||
            text_contains(lower, "new")) &&
           (text_contains(lower, "directory") ||
            text_contains(lower, "folder"));
}

static int request_is_remove_directory(const char *request)
{
    char lower[MAX_INPUT];

    lowercase_copy(lower, sizeof(lower), request);
    return (text_contains(lower, "remove") ||
            text_contains(lower, "delete")) &&
           (text_contains(lower, "directory") ||
            text_contains(lower, "folder"));
}

static int extract_request_name(const char *request, char *name, size_t name_size)
{
    return extract_word_after_phrase(request, "named", name, name_size) ||
           extract_word_after_phrase(request, "called", name, name_size) ||
           extract_word_after_phrase(request, "directory", name, name_size) ||
           extract_word_after_phrase(request, "folder", name, name_size);
}

static int word_is_command_name(const char *word)
{
    return find_command(word) != NULL ||
           strcmp(word, "help") == 0 ||
           strcmp(word, "version") == 0 ||
           strcmp(word, "exit") == 0 ||
           strcmp(word, "quit") == 0;
}

static int extract_command_name_from_request(const char *request,
                                             char *name,
                                             size_t name_size)
{
    char copy[MAX_INPUT];
    char *token;
    int saw_help = 0;

    snprintf(copy, sizeof(copy), "%s", request);
    token = strtok(copy, " \t\r\n,.;:?!()[]{}\"'");

    while (token != NULL) {
        sanitize_name(token);
        if (strcmp(token, "help") == 0) {
            saw_help = 1;
            token = strtok(NULL, " \t\r\n,.;:?!()[]{}\"'");
            continue;
        }
        if (word_is_command_name(token)) {
            snprintf(name, name_size, "%s", token);
            return 1;
        }
        token = strtok(NULL, " \t\r\n,.;:?!()[]{}\"'");
    }

    if (saw_help) {
        snprintf(name, name_size, "help");
        return 1;
    }

    return 0;
}

static int request_is_help_question(const char *lower)
{
    return text_contains(lower, "help") ||
           text_contains(lower, "usage") ||
           text_contains(lower, "option") ||
           text_contains(lower, "how do i use") ||
           text_contains(lower, "how to use");
}

static int request_is_explanation_question(const char *lower)
{
    return text_contains(lower, "what does") ||
           text_contains(lower, "what is") ||
           text_contains(lower, "explain") ||
           text_contains(lower, "describe");
}

static int request_is_option_question(const char *request)
{
    return text_contains(request, " -") ||
           text_contains(request, "--") ||
           text_contains(request, "flag") ||
           text_contains(request, "option") ||
           text_contains(request, "parameter") ||
           text_contains(request, "param");
}

static int extract_echo_text(const char *request, char *out, size_t out_size)
{
    static const char *phrases[] = {
        "print out",
        "print",
        "say",
        "write",
        "display",
        "show message",
        "output"
    };
    size_t index;

    for (index = 0; index < sizeof(phrases) / sizeof(phrases[0]); index++) {
        const char *start = strstr(request, phrases[index]);

        if (start != NULL) {
            start += strlen(phrases[index]);
            start = skip_spaces(start);
            if (strncmp(start, "the word ", 9) == 0) {
                start += 9;
            } else if (strncmp(start, "message ", 8) == 0) {
                start += 8;
            } else if (strncmp(start, "text ", 5) == 0) {
                start += 5;
            }
            snprintf(out, out_size, "%s", start);
            sanitize_echo_text(out);
            return out[0] != '\0';
        }
    }

    return 0;
}

static int extract_last_name(const char *request, char *name, size_t name_size)
{
    char copy[MAX_INPUT];
    char *token;
    char last[MAX_INPUT] = "";

    snprintf(copy, sizeof(copy), "%s", request);
    token = strtok(copy, " \t\r\n,;:?!()[]{}\"'");

    while (token != NULL) {
        sanitize_name(token);
        if (token[0] != '\0' &&
            strcmp(token, "the") != 0 &&
            strcmp(token, "a") != 0 &&
            strcmp(token, "an") != 0 &&
            strcmp(token, "of") != 0 &&
            strcmp(token, "in") != 0 &&
            strcmp(token, "file") != 0 &&
            strcmp(token, "directory") != 0 &&
            strcmp(token, "folder") != 0 &&
            !word_is_command_name(token)) {
            snprintf(last, sizeof(last), "%s", token);
        }
        token = strtok(NULL, " \t\r\n,;:?!()[]{}\"'");
    }

    if (last[0] == '\0') {
        return 0;
    }

    snprintf(name, name_size, "%s", last);
    return 1;
}

static int request_wants_json(const char *lower)
{
    return text_contains(lower, "json") ||
           text_contains(lower, "machine readable") ||
           text_contains(lower, "structured");
}

static int request_wants_reverse(const char *lower)
{
    return text_contains(lower, "reverse") ||
           text_contains(lower, "reversed") ||
           text_contains(lower, "backwards") ||
           text_contains(lower, "opposite order");
}

static int request_wants_ls(const char *lower)
{
    return text_contains(lower, "list") ||
           text_contains(lower, "files") ||
           text_contains(lower, "file names") ||
           text_contains(lower, "show directory");
}

static int build_ls_command_from_request(const char *lower,
                                         char *command,
                                         size_t command_size)
{
    if (!request_wants_ls(lower)) {
        return 0;
    }

    snprintf(command, command_size, "ls");

    if (request_wants_json(lower)) {
        strncat(command, " --json", command_size - strlen(command) - 1);
    }
    if (request_wants_reverse(lower)) {
        strncat(command, " -r", command_size - strlen(command) - 1);
    }
    if (text_contains(lower, "hidden") ||
        text_contains(lower, "all files") ||
        text_contains(lower, "dot files")) {
        strncat(command, " -a", command_size - strlen(command) - 1);
    }
    if (text_contains(lower, "long") ||
        text_contains(lower, "details") ||
        text_contains(lower, "detailed")) {
        strncat(command, " -l", command_size - strlen(command) - 1);
    }
    if (text_contains(lower, "recursive") ||
        text_contains(lower, "recursively") ||
        text_contains(lower, "subdirectories")) {
        strncat(command, " -R", command_size - strlen(command) - 1);
    }
    if (text_contains(lower, "size")) {
        strncat(command, " -S", command_size - strlen(command) - 1);
    } else if (text_contains(lower, "time") ||
               text_contains(lower, "newest") ||
               text_contains(lower, "recent") ||
               text_contains(lower, "modified")) {
        strncat(command, " -t", command_size - strlen(command) - 1);
    }
    if (text_contains(lower, "color") ||
        text_contains(lower, "colour")) {
        strncat(command, " --color", command_size - strlen(command) - 1);
    }

    return 1;
}

static int fallback_nl_to_command(const char *request, char *command, size_t command_size)
{
    char lower[MAX_INPUT];
    char echo_text[MAX_INPUT];
    char command_name[MAX_INPUT];
    char target[MAX_INPUT];

    lowercase_copy(lower, sizeof(lower), request);

    if (request_is_option_question(lower)) {
        return 0;
    } else if ((request_is_help_question(lower) ||
         request_is_explanation_question(lower)) &&
        extract_command_name_from_request(lower, command_name, sizeof(command_name))) {
        snprintf(command, command_size, "help %s", command_name);
    } else if (text_contains(lower, "date") || text_contains(lower, "today")) {
        snprintf(command, command_size, "localdate");
    } else if (extract_echo_text(request, echo_text, sizeof(echo_text))) {
        snprintf(command, command_size, "echo %s", echo_text);
    } else if ((text_contains(lower, "create") ||
                text_contains(lower, "make") ||
                text_contains(lower, "new")) &&
               (text_contains(lower, "directory") ||
                text_contains(lower, "folder"))) {
        command_with_optional_name(command, command_size, "mkdir", request, "new_directory");
    } else if ((text_contains(lower, "remove") ||
                text_contains(lower, "delete")) &&
               (text_contains(lower, "directory") ||
                text_contains(lower, "folder"))) {
        command_with_optional_name(command, command_size, "rmdir", request, "old_directory");
    } else if (text_contains(lower, "current directory") ||
               text_contains(lower, "where am i") ||
               text_contains(lower, "working directory")) {
        snprintf(command, command_size, "pwd");
    } else if (build_ls_command_from_request(lower, command, command_size)) {
        return 1;
    } else if (text_contains(lower, "who am i") ||
               text_contains(lower, "username") ||
               text_contains(lower, "user name")) {
        snprintf(command, command_size, "whoami");
    } else if (text_contains(lower, "system") ||
               text_contains(lower, "kernel") ||
               text_contains(lower, "machine")) {
        snprintf(command, command_size, "uname -a");
    } else if (text_contains(lower, "identity") ||
               text_contains(lower, "uid") ||
               text_contains(lower, "gid")) {
        snprintf(command, command_size, "id");
    } else if (text_contains(lower, "clear") &&
               text_contains(lower, "screen")) {
        snprintf(command, command_size, "clear");
    } else if ((text_contains(lower, "count") ||
                text_contains(lower, "word count") ||
                text_contains(lower, "line count")) &&
               extract_last_name(request, target, sizeof(target))) {
        snprintf(command, command_size, "wc %s", target);
    } else if ((text_contains(lower, "first") ||
                text_contains(lower, "beginning")) &&
               extract_last_name(request, target, sizeof(target))) {
        snprintf(command, command_size, "head %s", target);
    } else if ((text_contains(lower, "last") ||
                text_contains(lower, "end of")) &&
               extract_last_name(request, target, sizeof(target))) {
        snprintf(command, command_size, "tail %s", target);
    } else if ((text_contains(lower, "disk usage") ||
                text_contains(lower, "size of")) &&
               extract_last_name(request, target, sizeof(target))) {
        snprintf(command, command_size, "du %s", target);
    } else if ((text_contains(lower, "show file") ||
                text_contains(lower, "read file") ||
                text_contains(lower, "view file")) &&
               extract_last_name(request, target, sizeof(target))) {
        snprintf(command, command_size, "cat %s", target);
    } else if (text_contains(lower, "help")) {
        snprintf(command, command_size, "help");
    } else {
        return 0;
    }

    return 1;
}

static void append_shell_quoted(char *out, size_t out_size, const char *text)
{
    size_t used = strlen(out);

    if (used + 1 < out_size) {
        out[used++] = '\'';
        out[used] = '\0';
    }

    while (*text != '\0' && used + 1 < out_size) {
        if (*text == '\'') {
            if (used + 4 >= out_size) {
                break;
            }
            out[used++] = '\'';
            out[used++] = '\\';
            out[used++] = '\'';
            out[used++] = '\'';
        } else {
            out[used++] = *text;
        }
        out[used] = '\0';
        text++;
    }

    if (used + 1 < out_size) {
        out[used++] = '\'';
        out[used] = '\0';
    }
}

static int read_helper_command(const char *request, char *command, size_t command_size)
{
    const char *helper = getenv("MYSH_LLM_HELPER");
    char shell_command[MAX_INPUT * 2];
    char prompt[MAX_INPUT * 2];
    FILE *pipe;

    if (helper == NULL || *helper == '\0') {
        if (access("./ollama_llm_helper.sh", X_OK) == 0) {
            helper = "./ollama_llm_helper.sh";
        } else {
            return 0;
        }
    }

    snprintf(prompt, sizeof(prompt),
             "Convert this request to one safe busybox_shell command. "
             "Return only the command, no markdown: %s",
             request);

    snprintf(shell_command, sizeof(shell_command), "%s ", helper);
    append_shell_quoted(shell_command, sizeof(shell_command), prompt);

    pipe = popen(shell_command, "r");
    if (pipe == NULL) {
        return 0;
    }

    if (fgets(command, command_size, pipe) == NULL) {
        pclose(pipe);
        return 0;
    }

    pclose(pipe);
    trim_newline(command);
    return !is_blank_line(command);
}

static int command_is_safe_to_dispatch(char *command)
{
    char copy[MAX_INPUT];
    char *argv[MAX_ARGS];
    int argc;
    size_t index;

    for (index = 0; command[index] != '\0'; index++) {
        if (command[index] == ';' ||
            command[index] == '&' ||
            command[index] == '|' ||
            command[index] == '<' ||
            command[index] == '>' ||
            command[index] == '`' ||
            command[index] == '$' ||
            command[index] == '(' ||
            command[index] == ')' ||
            command[index] == '\n' ||
            command[index] == '\r') {
            return 0;
        }
    }

    snprintf(copy, sizeof(copy), "%s", command);
    argc = split_line(copy, argv, MAX_ARGS);

    if (argc == 0) {
        return 0;
    }

    if (strcmp(argv[0], "help") == 0 ||
        strcmp(argv[0], "version") == 0 ||
        strcmp(argv[0], "--version") == 0 ||
        strcmp(argv[0], "exit") == 0 ||
        strcmp(argv[0], "quit") == 0) {
        return 1;
    }

    return find_command(argv[0]) != NULL;
}

static int dispatch_command_line(const char *command)
{
    int status;

    status = dispatch_bnfc_line(command);
    if (status == 2 && line_contains_shell_syntax(command)) {
        return 1;
    }
    return status;
}

static int dispatch_argv_line_with_bnfc(int argc, char **argv)
{
    char line[MAX_INPUT];
    size_t used = 0;
    int index;
    int status;

    line[0] = '\0';
    for (index = 0; index < argc; index++) {
        int written;

        written = snprintf(line + used, sizeof(line) - used,
                           "%s%s", index == 0 ? "" : " ", argv[index]);
        if (written < 0 || (size_t) written >= sizeof(line) - used) {
            fprintf(stderr, "Command line too long\n");
            return 1;
        }
        used += (size_t) written;
    }

    status = dispatch_bnfc_line(line);
    if (status == 2 && line_contains_shell_syntax(line)) {
        return 1;
    }
    return status;
}

static int ask_yes_no(const char *prompt)
{
    char answer[16];

    printf("%s", prompt);
    fflush(stdout);

    if (fgets(answer, sizeof(answer), stdin) == NULL) {
        return 0;
    }

    return answer[0] == 'y' || answer[0] == 'Y';
}

static int ask_for_name(const char *prompt, char *name, size_t name_size)
{
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(name, name_size, stdin) == NULL) {
        return 0;
    }

    trim_newline(name);
    sanitize_name(name);
    return name[0] != '\0';
}

static int clarify_missing_nl_arguments(const char *request,
                                        int interactive_terminal,
                                        char *command,
                                        size_t command_size)
{
    char name[MAX_INPUT];

    if (!interactive_terminal) {
        return 0;
    }

    if (request_is_create_directory(request) &&
        !extract_request_name(request, name, sizeof(name))) {
        if (!ask_for_name("Directory name: ", name, sizeof(name))) {
            printf("No directory name provided.\n");
            return -1;
        }
        snprintf(command, command_size, "mkdir %s", name);
        return 1;
    }

    if (request_is_remove_directory(request) &&
        !extract_request_name(request, name, sizeof(name))) {
        if (!ask_for_name("Directory name to remove: ", name, sizeof(name))) {
            printf("No directory name provided.\n");
            return -1;
        }
        snprintf(command, command_size, "rmdir %s", name);
        return 1;
    }

    return 0;
}

static int try_ollama_after_reject(const char *request,
                                   const char *previous_command,
                                   char *accepted_command,
                                   size_t accepted_command_size)
{
    char ollama_command[MAX_INPUT];
    int status;

    printf("Asking Ollama for another suggestion...\n");

    if (!read_helper_command(request, ollama_command, sizeof(ollama_command))) {
        printf("No alternate AI suggestion available. Set MYSH_LLM_HELPER to use Ollama.\n");
        return 0;
    }

    if (!command_is_safe_to_dispatch(ollama_command)) {
        fprintf(stderr, "Ollama suggested an unsupported command: %s\n", ollama_command);
        return 1;
    }

    if (strcmp(previous_command, ollama_command) == 0) {
        printf("Ollama suggested the same command: %s\n", ollama_command);
        return 0;
    }

    printf("Ollama suggestion: %s\n", ollama_command);
    if (!ask_yes_no("Run it? [y/N] ")) {
        return 0;
    }

    snprintf(accepted_command, accepted_command_size, "%s", ollama_command);
    status = dispatch_command_line(ollama_command);
    return status < 0 ? 0 : status;
}

static int handle_natural_language_request(const char *line, int interactive_terminal)
{
    const char *request = skip_spaces(line + 1);
    char command[MAX_INPUT];
    int clarified;
    int cacheable = 1;
    int used_helper = 0;
    int status;

    if (is_blank_line(request)) {
        printf("Usage: @ describe what you want the shell to do\n");
        return 0;
    }

    if (lookup_nl_cache(request, command, sizeof(command))) {
        printf("AI suggestion: %s\n", command);
        if (interactive_terminal) {
            if (!ask_yes_no("Run it? [y/N] ")) {
                status = try_ollama_after_reject(request, command, command, sizeof(command));
                if (status == 0) {
                    store_nl_cache(request, command);
                }
                return status;
            }
            status = dispatch_command_line(command);
            return status < 0 ? 0 : status;
        }
        printf("Not running suggestion in non-interactive mode.\n");
        return 0;
    }

    clarified = clarify_missing_nl_arguments(request, interactive_terminal,
                                             command, sizeof(command));
    if (clarified < 0) {
        return 1;
    }
    if (clarified > 0) {
        cacheable = 0;
    }

    if (clarified == 0 &&
        !fallback_nl_to_command(request, command, sizeof(command))) {
        if (!read_helper_command(request, command, sizeof(command))) {
            printf("AI suggestion unavailable. Set MYSH_LLM_HELPER or try a simpler request.\n");
            return 1;
        }
        used_helper = 1;
    }

    if (!command_is_safe_to_dispatch(command)) {
        fprintf(stderr, "AI suggested an unsupported command: %s\n", command);
        return 1;
    }

    if (cacheable) {
        store_nl_cache(request, command);
    }

    printf("AI suggestion: %s\n", command);

    if (interactive_terminal) {
        if (!ask_yes_no("Run it? [y/N] ")) {
            if (!used_helper) {
                status = try_ollama_after_reject(request, command, command, sizeof(command));
                if (status == 0 && cacheable) {
                    store_nl_cache(request, command);
                }
                return status;
            }
            return 0;
        }
    } else {
        printf("Not running suggestion in non-interactive mode.\n");
        return 0;
    }

    status = dispatch_command_line(command);
    return status < 0 ? 0 : status;
}

static int run_interactive_shell(void)
{
    char line[MAX_INPUT];
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

        if (line[0] == '@') {
            status = handle_natural_language_request(line, interactive_terminal);
            continue;
        }

        status = dispatch_bnfc_line(line);
        if (status == 2 && line_contains_shell_syntax(line)) {
            status = 1;
        }

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

    shell_program_path = argv[0];
    register_all_builtin_commands();

    if (argc >= 2 && strcmp(argv[1], INTERNAL_RUN_COMMAND_FLAG) == 0) {
        status = dispatch_command(argc - 2, argv + 2);
        free_shell_variables();
        return status < 0 ? 0 : status;
    }

    if (argc < 2) {
        status = run_interactive_shell();
        free_shell_variables();
        return status;
    }

    status = dispatch_argv_line_with_bnfc(argc - 1, argv + 1);
    free_shell_variables();
    return status < 0 ? 0 : status;
}
