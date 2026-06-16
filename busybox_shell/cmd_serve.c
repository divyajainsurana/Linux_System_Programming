#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "cmd_serve.h"
#include "cmd_spec.h"
#include "json_utils.h"

#define SERVE_DEFAULT_HOST "0.0.0.0"
#define SERVE_DEFAULT_PORT 9000
#define SERVE_DEFAULT_TIMEOUT_SECONDS 5
#define SERVE_DEFAULT_MAX_REQUESTS 8
#define SERVE_BACKLOG 8
#define SERVE_LINE_SIZE 2048
#define SERVE_OUTPUT_SIZE 16384
#define SERVE_MAX_ARGS 64
#define SERVE_MAX_ALLOWLIST 32

struct serve_config {
    char host[64];
    int port;
    int timeout_seconds;
    int max_requests;
    int http_mode;
    int once;
    const char *allowed[SERVE_MAX_ALLOWLIST];
    int allowed_count;
};

static const char *default_allowed[] = {
    "get_time",
    "list_files",
    "delete_older_than_days",
    "http_get",
    "ls",
    "pwd",
    "localdate",
    "mkdir",
};

static int parse_int_range(const char *text, int min, int max, int *out)
{
    char *endptr;
    long value;

    errno = 0;
    value = strtol(text, &endptr, 10);
    if (errno != 0 || endptr == text || *endptr != '\0' ||
        value < min || value > max) {
        return 0;
    }

    *out = (int) value;
    return 1;
}

static void trim_line(char *line)
{
    size_t len;

    while (isspace((unsigned char) *line)) {
        memmove(line, line + 1, strlen(line));
    }

    len = strlen(line);
    while (len > 0 && isspace((unsigned char) line[len - 1])) {
        line[--len] = '\0';
    }
}

static int token_is_safe(const char *text)
{
    const char *cursor;

    if (text == NULL || text[0] == '\0') {
        return 0;
    }

    for (cursor = text; *cursor != '\0'; cursor++) {
        unsigned char ch = (unsigned char) *cursor;

        if (iscntrl(ch) || strchr(";|&<>`$(){}[]\\\"'", *cursor) != NULL) {
            return 0;
        }
    }

    return 1;
}

static void config_add_allowed(struct serve_config *config, const char *name)
{
    if (config->allowed_count >= SERVE_MAX_ALLOWLIST || name == NULL ||
        name[0] == '\0') {
        return;
    }
    config->allowed[config->allowed_count++] = name;
}

static void config_set_allowed_csv(struct serve_config *config, char *csv)
{
    char *token;

    config->allowed_count = 0;
    for (token = strtok(csv, ","); token != NULL; token = strtok(NULL, ",")) {
        trim_line(token);
        if (token[0] != '\0') {
            config_add_allowed(config, token);
        }
    }
}

static int command_is_allowed(const struct serve_config *config, const char *name)
{
    int index;

    for (index = 0; index < config->allowed_count; index++) {
        if (strcmp(config->allowed[index], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int append_text(char *out, size_t out_size, const char *text)
{
    size_t used = strlen(out);
    int written;

    if (used >= out_size) {
        return 0;
    }

    written = snprintf(out + used, out_size - used, "%s", text);
    return written >= 0 && (size_t) written < out_size - used;
}

static void send_response(int fd, const char *body)
{
    write(fd, body, strlen(body));
}

static void send_initialize(char *out, size_t out_size,
                            const struct serve_config *config)
{
    snprintf(out,
             out_size,
             "{\"type\":\"initialize_result\",\"server\":\"AiShell service\","
             "\"version\":\"1.0\",\"protocol\":\"%s\","
             "\"timeout_seconds\":%d,\"max_requests\":%d}\n",
             config->http_mode ? "http" : "line-rpc",
             config->timeout_seconds,
             config->max_requests);
}

static void append_json_string_to_buffer(char *out, size_t out_size,
                                         const char *text)
{
    FILE *stream;
    char *buffer = NULL;
    size_t size = 0;

    stream = open_memstream(&buffer, &size);
    if (stream == NULL) {
        append_text(out, out_size, "\"\"");
        return;
    }
    json_print_string(stream, text);
    fclose(stream);
    if (buffer != NULL) {
        append_text(out, out_size, buffer);
        free(buffer);
    }
}

static void append_tool_entry(char *out, size_t out_size,
                              const char *name, const char *summary,
                              int *first)
{
    if (!*first) {
        append_text(out, out_size, ",");
    }
    *first = 0;
    append_text(out, out_size, "{\"name\":");
    append_json_string_to_buffer(out, out_size, name);
    append_text(out, out_size, ",\"summary\":");
    append_json_string_to_buffer(out, out_size, summary);
    append_text(out, out_size, "}");
}

static void send_list_tools(char *out, size_t out_size,
                            const struct serve_config *config)
{
    int index;
    int first = 1;

    out[0] = '\0';
    append_text(out, out_size, "{\"type\":\"list_tools_result\",\"tools\":[");
    if (command_is_allowed(config, "get_time")) {
        append_tool_entry(out, out_size, "get_time",
                          "return the server local timestamp", &first);
    }
    if (command_is_allowed(config, "list_files")) {
        append_tool_entry(out, out_size, "list_files",
                          "list files using the ls service", &first);
    }
    if (command_is_allowed(config, "delete_older_than_days")) {
        append_tool_entry(out, out_size, "delete_older_than_days",
                          "dry-run or delete files older than N days", &first);
    }
    if (command_is_allowed(config, "http_get")) {
        append_tool_entry(out, out_size, "http_get",
                          "fetch a small preview from an HTTP or HTTPS URL", &first);
    }
    for (index = 0; index < config->allowed_count; index++) {
        const cmd_spec_t *spec = find_command(config->allowed[index]);

        if (spec != NULL) {
            append_tool_entry(out, out_size, spec->name, spec->summary, &first);
        }
    }
    append_text(out, out_size, "]}\n");
}

static int split_request(char *line, char **argv, int max_args)
{
    int argc = 0;
    char *token;

    for (token = strtok(line, " \t\r\n"); token != NULL && argc < max_args - 1;
         token = strtok(NULL, " \t\r\n")) {
        argv[argc++] = token;
    }
    argv[argc] = NULL;
    return argc;
}

static char *arg_value(char *arg)
{
    char *colon = strchr(arg, ':');

    if (colon == NULL) {
        return arg;
    }
    return colon + 1;
}

static const char *named_arg_value(int argc, char **argv, const char *name)
{
    int index;
    size_t name_length = strlen(name);

    for (index = 0; index < argc; index++) {
        if (strncmp(argv[index], name, name_length) == 0 &&
            argv[index][name_length] == ':') {
            return argv[index] + name_length + 1;
        }
    }
    return NULL;
}

static void write_error_body(char *out, size_t out_size, const char *message)
{
    out[0] = '\0';
    append_text(out, out_size, "{\"type\":\"error\",\"error\":");
    append_json_string_to_buffer(out, out_size, message);
    append_text(out, out_size, "}\n");
}

static int parse_bool_arg(const char *text, int default_value)
{
    if (text == NULL) {
        return default_value;
    }
    if (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 ||
        strcmp(text, "yes") == 0) {
        return 1;
    }
    if (strcmp(text, "0") == 0 || strcmp(text, "false") == 0 ||
        strcmp(text, "no") == 0) {
        return 0;
    }
    return default_value;
}

static void write_ok_body(char *out, size_t out_size, const char *type)
{
    snprintf(out, out_size, "{\"type\":\"%s\"}\n", type);
}

static void send_get_time(char *out, size_t out_size)
{
    char timestamp[64];
    time_t now = time(NULL);
    struct tm local_time;

    if (localtime_r(&now, &local_time) == NULL) {
        write_error_body(out, out_size, "failed to read local time");
        return;
    }

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &local_time);
    out[0] = '\0';
    append_text(out, out_size,
                "{\"type\":\"tool_result\",\"tool\":\"get_time\",\"time\":");
    append_json_string_to_buffer(out, out_size, timestamp);
    append_text(out, out_size, "}\n");
}

static int append_file_result(char *out, size_t out_size, const char *path,
                              int deleted, int *first)
{
    if (!*first) {
        append_text(out, out_size, ",");
    }
    *first = 0;
    append_text(out, out_size, "{\"path\":");
    append_json_string_to_buffer(out, out_size, path);
    append_text(out, out_size, ",\"deleted\":");
    append_text(out, out_size, deleted ? "true" : "false");
    append_text(out, out_size, "}");
    return 1;
}

static void send_delete_older_than_days(char *out, size_t out_size,
                                        int argc, char **argv)
{
    const char *path = named_arg_value(argc, argv, "path");
    const char *days_text = named_arg_value(argc, argv, "days");
    const char *dry_run_text = named_arg_value(argc, argv, "dry_run");
    const char *confirm = named_arg_value(argc, argv, "confirm");
    DIR *dir;
    struct dirent *entry;
    time_t now;
    time_t cutoff;
    int days;
    int dry_run;
    int matched = 0;
    int deleted = 0;
    int first = 1;

    if (path == NULL) {
        path = ".";
    }
    if (!token_is_safe(path)) {
        write_error_body(out, out_size, "unsafe path");
        return;
    }
    if (days_text == NULL || !parse_int_range(days_text, 1, 3650, &days)) {
        write_error_body(out, out_size, "days must be an integer from 1 to 3650");
        return;
    }

    dry_run = parse_bool_arg(dry_run_text, 1);
    if (!dry_run && (confirm == NULL || strcmp(confirm, "DELETE") != 0)) {
        write_error_body(out, out_size,
                         "actual deletion requires dry_run:false and confirm:DELETE");
        return;
    }

    dir = opendir(path);
    if (dir == NULL) {
        write_error_body(out, out_size, "could not open directory");
        return;
    }

    now = time(NULL);
    cutoff = now - (time_t) days * 24 * 60 * 60;

    out[0] = '\0';
    append_text(out, out_size,
                "{\"type\":\"tool_result\",\"tool\":\"delete_older_than_days\",");
    append_text(out, out_size, "\"dry_run\":");
    append_text(out, out_size, dry_run ? "true" : "false");
    append_text(out, out_size, ",\"days\":");
    {
        char number[32];

        snprintf(number, sizeof(number), "%d", days);
        append_text(out, out_size, number);
    }
    append_text(out, out_size, ",\"files\":[");

    while ((entry = readdir(dir)) != NULL) {
        char full_path[2048];
        struct stat st;
        int did_delete = 0;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (snprintf(full_path, sizeof(full_path), "%s/%s",
                     path, entry->d_name) >= (int) sizeof(full_path)) {
            continue;
        }

        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        if (st.st_mtime > cutoff) {
            continue;
        }

        matched++;
        if (!dry_run && unlink(full_path) == 0) {
            did_delete = 1;
            deleted++;
        }
        append_file_result(out, out_size, full_path, did_delete, &first);
    }

    closedir(dir);
    append_text(out, out_size, "],\"matched\":");
    {
        char number[32];

        snprintf(number, sizeof(number), "%d", matched);
        append_text(out, out_size, number);
    }
    append_text(out, out_size, ",\"deleted\":");
    {
        char number[32];

        snprintf(number, sizeof(number), "%d", deleted);
        append_text(out, out_size, number);
    }
    append_text(out, out_size, "}\n");
}

static int url_is_safe(const char *url)
{
    const char *cursor;

    if (url == NULL || url[0] == '\0') {
        return 0;
    }

    if (strncmp(url, "http://", 7) != 0 &&
        strncmp(url, "https://", 8) != 0) {
        return 0;
    }

    for (cursor = url; *cursor != '\0'; cursor++) {
        unsigned char ch = (unsigned char) *cursor;

        if (isspace(ch) || iscntrl(ch) ||
            strchr("'\"`$(){}[]\\|<>;", *cursor) != NULL) {
            return 0;
        }
    }

    return 1;
}

static void append_shell_single_quoted(char *out, size_t out_size,
                                       const char *text)
{
    append_text(out, out_size, "'");
    while (*text != '\0') {
        if (*text == '\'') {
            append_text(out, out_size, "'\\''");
        } else {
            char one[2];

            one[0] = *text;
            one[1] = '\0';
            append_text(out, out_size, one);
        }
        text++;
    }
    append_text(out, out_size, "'");
}

static void send_http_get(char *out, size_t out_size, int argc, char **argv)
{
    const char *url = named_arg_value(argc, argv, "url");
    const char *limit_text = named_arg_value(argc, argv, "limit");
    const char *timeout_text = named_arg_value(argc, argv, "timeout");
    char command[4096];
    char chunk[512];
    FILE *pipe;
    int limit = 4096;
    int timeout = 5;
    int bytes = 0;

    if (url == NULL && argc > 0) {
        url = arg_value(argv[0]);
    }

    if (!url_is_safe(url)) {
        write_error_body(out, out_size,
                         "url must start with http:// or https:// and contain no shell metacharacters");
        return;
    }

    if (limit_text != NULL && !parse_int_range(limit_text, 256, 12000, &limit)) {
        write_error_body(out, out_size, "limit must be an integer from 256 to 12000");
        return;
    }
    if (timeout_text != NULL && !parse_int_range(timeout_text, 1, 20, &timeout)) {
        write_error_body(out, out_size, "timeout must be an integer from 1 to 20");
        return;
    }

    command[0] = '\0';
    snprintf(command, sizeof(command),
             "curl -L -sS --max-time %d --connect-timeout %d --range 0-%d ",
             timeout,
             timeout,
             limit - 1);
    append_shell_single_quoted(command, sizeof(command), url);
    append_text(command, sizeof(command), " 2>&1");

    pipe = popen(command, "r");
    if (pipe == NULL) {
        write_error_body(out, out_size, "failed to run curl");
        return;
    }

    out[0] = '\0';
    append_text(out, out_size, "{\"type\":\"tool_result\",\"tool\":\"http_get\",\"url\":");
    append_json_string_to_buffer(out, out_size, url);
    append_text(out, out_size, ",\"limit\":");
    {
        char number[32];

        snprintf(number, sizeof(number), "%d", limit);
        append_text(out, out_size, number);
    }
    append_text(out, out_size, ",\"content\":");
    append_text(out, out_size, "\"");

    while (bytes < limit && fgets(chunk, sizeof(chunk), pipe) != NULL) {
        char *cursor;

        for (cursor = chunk; *cursor != '\0' && bytes < limit; cursor++) {
            char escaped[8];

            bytes++;
            if (*cursor == '"' || *cursor == '\\') {
                snprintf(escaped, sizeof(escaped), "\\%c", *cursor);
                append_text(out, out_size, escaped);
            } else if (*cursor == '\n') {
                append_text(out, out_size, "\\n");
            } else if (*cursor == '\r') {
                append_text(out, out_size, "\\r");
            } else if (*cursor == '\t') {
                append_text(out, out_size, "\\t");
            } else if ((unsigned char) *cursor < 0x20) {
                snprintf(escaped, sizeof(escaped), "\\u%04x",
                         (unsigned char) *cursor);
                append_text(out, out_size, escaped);
            } else {
                escaped[0] = *cursor;
                escaped[1] = '\0';
                append_text(out, out_size, escaped);
            }
        }
    }

    pclose(pipe);
    append_text(out, out_size, "\",\"bytes\":");
    {
        char number[32];

        snprintf(number, sizeof(number), "%d", bytes);
        append_text(out, out_size, number);
    }
    append_text(out, out_size, "}\n");
}

static int run_allowed_command(char *out, size_t out_size, const char *tool,
                               int argc, char **argv)
{
    const cmd_spec_t *spec;
    char *cmd_argv[SERVE_MAX_ARGS];
    int cmd_argc = 0;
    int index;
    int saved_stdout;
    int saved_stderr;
    int status;
    FILE *capture;
    char chunk[512];

    spec = find_command(tool);
    if (spec == NULL) {
        write_error_body(out, out_size, "unknown or unavailable tool");
        return 1;
    }

    cmd_argv[cmd_argc++] = (char *) spec->name;
    for (index = 0; index < argc && cmd_argc < SERVE_MAX_ARGS - 1; index++) {
        char *value = arg_value(argv[index]);

        if (!token_is_safe(value)) {
            write_error_body(out, out_size, "unsafe or malformed argument");
            return 1;
        }
        cmd_argv[cmd_argc++] = value;
    }
    cmd_argv[cmd_argc] = NULL;

    capture = tmpfile();
    if (capture == NULL) {
        write_error_body(out, out_size, "failed to prepare command output");
        return 1;
    }

    fflush(stdout);
    fflush(stderr);
    saved_stdout = dup(STDOUT_FILENO);
    saved_stderr = dup(STDERR_FILENO);
    if (saved_stdout < 0 || saved_stderr < 0) {
        write_error_body(out, out_size, "failed to prepare command output");
        if (saved_stdout >= 0) {
            close(saved_stdout);
        }
        if (saved_stderr >= 0) {
            close(saved_stderr);
        }
        fclose(capture);
        return 1;
    }

    dup2(fileno(capture), STDOUT_FILENO);
    dup2(fileno(capture), STDERR_FILENO);
    status = spec->run(cmd_argc, cmd_argv);
    fflush(stdout);
    fflush(stderr);
    dup2(saved_stdout, STDOUT_FILENO);
    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stdout);
    close(saved_stderr);

    rewind(capture);
    out[0] = '\0';
    append_text(out, out_size, "{\"type\":\"tool_result\",\"tool\":");
    append_json_string_to_buffer(out, out_size, spec->name);
    snprintf(out + strlen(out), out_size - strlen(out),
             ",\"status\":%d,\"output\":", status);
    append_text(out, out_size, "\"");
    while (fgets(chunk, sizeof(chunk), capture) != NULL) {
        char *cursor;

        for (cursor = chunk; *cursor != '\0'; cursor++) {
            char escaped[8];

            if (*cursor == '"' || *cursor == '\\') {
                snprintf(escaped, sizeof(escaped), "\\%c", *cursor);
                append_text(out, out_size, escaped);
            } else if (*cursor == '\n') {
                append_text(out, out_size, "\\n");
            } else if (*cursor == '\r') {
                append_text(out, out_size, "\\r");
            } else if ((unsigned char) *cursor < 0x20) {
                snprintf(escaped, sizeof(escaped), "\\u%04x",
                         (unsigned char) *cursor);
                append_text(out, out_size, escaped);
            } else {
                escaped[0] = *cursor;
                escaped[1] = '\0';
                append_text(out, out_size, escaped);
            }
        }
    }
    append_text(out, out_size, "\"}\n");
    fclose(capture);
    return status == 0 ? 0 : 1;
}

static void handle_call_tool(char *out, size_t out_size,
                             const struct serve_config *config,
                             int argc, char **argv)
{
    const char *tool;

    if (argc < 2) {
        write_error_body(out, out_size, "call_tool requires a tool name");
        return;
    }

    tool = argv[1];
    if (!token_is_safe(tool)) {
        write_error_body(out, out_size, "unsafe tool name");
        return;
    }

    if (!command_is_allowed(config, tool)) {
        write_error_body(out, out_size, "tool is disabled by configuration");
        return;
    }

    if (strcmp(tool, "get_time") == 0) {
        send_get_time(out, out_size);
        return;
    }

    if (strcmp(tool, "list_files") == 0) {
        char *mapped_argv[2];
        char *path = ".";

        if (argc >= 3) {
            path = arg_value(argv[2]);
        }
        mapped_argv[0] = path;
        mapped_argv[1] = NULL;
        run_allowed_command(out, out_size, "ls", 1, mapped_argv);
        return;
    }

    if (strcmp(tool, "delete_older_than_days") == 0) {
        send_delete_older_than_days(out, out_size, argc - 2, argv + 2);
        return;
    }

    if (strcmp(tool, "http_get") == 0) {
        send_http_get(out, out_size, argc - 2, argv + 2);
        return;
    }

    run_allowed_command(out, out_size, tool, argc - 2, argv + 2);
}

static void handle_request_line(char *out, size_t out_size,
                                const struct serve_config *config, char *line)
{
    char *argv[SERVE_MAX_ARGS];
    int argc;

    trim_line(line);
    if (line[0] == '\0') {
        write_error_body(out, out_size, "empty request");
        return;
    }

    argc = split_request(line, argv, SERVE_MAX_ARGS);
    if (argc == 0) {
        write_error_body(out, out_size, "empty request");
    } else if (strcmp(argv[0], "initialize") == 0) {
        send_initialize(out, out_size, config);
    } else if (strcmp(argv[0], "list_tools") == 0) {
        send_list_tools(out, out_size, config);
    } else if (strcmp(argv[0], "call_tool") == 0) {
        handle_call_tool(out, out_size, config, argc, argv);
    } else if (strcmp(argv[0], "quit") == 0) {
        write_ok_body(out, out_size, "goodbye");
    } else {
        write_error_body(out, out_size, "unknown method");
    }
}

static void handle_client(int fd, const struct serve_config *config)
{
    FILE *in;
    char line[SERVE_LINE_SIZE];
    char body[SERVE_OUTPUT_SIZE];
    int requests = 0;
    struct timeval timeout;

    timeout.tv_sec = config->timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    in = fdopen(dup(fd), "r");
    if (in == NULL) {
        close(fd);
        return;
    }

    while (requests < config->max_requests && fgets(line, sizeof(line), in) != NULL) {
        requests++;
        handle_request_line(body, sizeof(body), config, line);
        send_response(fd, body);
        if (strncmp(line, "quit", 4) == 0) {
            break;
        }
    }

    fclose(in);
    close(fd);
}

static int hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static void url_decode(char *text)
{
    char *readp = text;
    char *writep = text;

    while (*readp != '\0') {
        if (*readp == '%' && isxdigit((unsigned char) readp[1]) &&
            isxdigit((unsigned char) readp[2])) {
            int high = hex_value(readp[1]);
            int low = hex_value(readp[2]);

            *writep++ = (char) ((high << 4) | low);
            readp += 3;
        } else if (*readp == '+') {
            *writep++ = ' ';
            readp++;
        } else {
            *writep++ = *readp++;
        }
    }
    *writep = '\0';
}

static const char *http_status_for_body(const char *body)
{
    return strstr(body, "\"type\":\"error\"") != NULL ? "400 Bad Request" : "200 OK";
}

static void send_http_response(int fd, const char *body)
{
    const char *status = http_status_for_body(body);

    dprintf(fd,
            "HTTP/1.1 %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n"
            "\r\n",
            status,
            strlen(body));
    send_response(fd, body);
}

static int append_request_token(char *request, size_t request_size,
                                const char *token)
{
    size_t used = strlen(request);
    int written;

    if (used > 0) {
        written = snprintf(request + used, request_size - used, " %s", token);
    } else {
        written = snprintf(request + used, request_size - used, "%s", token);
    }
    return written >= 0 && (size_t) written < request_size - used;
}

static void build_call_request_from_query(char *request, size_t request_size,
                                          char *query)
{
    char *pair;
    char tool[128] = "";
    char args[SERVE_LINE_SIZE] = "";

    for (pair = strtok(query, "&"); pair != NULL; pair = strtok(NULL, "&")) {
        char *equals = strchr(pair, '=');
        char *key;
        char *value;

        if (equals == NULL) {
            continue;
        }
        *equals = '\0';
        key = pair;
        value = equals + 1;
        url_decode(key);
        url_decode(value);

        if (strcmp(key, "tool") == 0) {
            snprintf(tool, sizeof(tool), "%s", value);
        } else if (strcmp(key, "arg") == 0 || strcmp(key, "args") == 0) {
            append_request_token(args, sizeof(args), value);
        } else if (strncmp(key, "arg", 3) == 0) {
            append_request_token(args, sizeof(args), value);
        }
    }

    if (tool[0] == '\0') {
        snprintf(request, request_size, "call_tool");
    } else if (args[0] == '\0') {
        snprintf(request, request_size, "call_tool %s", tool);
    } else {
        snprintf(request, request_size, "call_tool %s %s", tool, args);
    }
}

static void handle_http_client(int fd, const struct serve_config *config)
{
    FILE *in;
    char request_line[SERVE_LINE_SIZE];
    char method[16];
    char target[SERVE_LINE_SIZE];
    char version[32];
    char service_request[SERVE_LINE_SIZE];
    char body[SERVE_OUTPUT_SIZE];
    char *query;
    char *path;
    struct timeval timeout;

    timeout.tv_sec = config->timeout_seconds;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    in = fdopen(dup(fd), "r");
    if (in == NULL) {
        close(fd);
        return;
    }

    if (fgets(request_line, sizeof(request_line), in) == NULL) {
        fclose(in);
        close(fd);
        return;
    }

    if (sscanf(request_line, "%15s %2047s %31s", method, target, version) != 3 ||
        strncmp(version, "HTTP/", 5) != 0) {
        handle_request_line(body, sizeof(body), config, request_line);
        send_response(fd, body);
        fclose(in);
        close(fd);
        return;
    }

    if (strcmp(method, "GET") != 0) {
        write_error_body(body, sizeof(body), "only HTTP GET is supported");
        send_http_response(fd, body);
        fclose(in);
        close(fd);
        return;
    }

    path = target;
    query = strchr(target, '?');
    if (query != NULL) {
        *query++ = '\0';
    }

    if (strcmp(path, "/") == 0 || strcmp(path, "/initialize") == 0) {
        snprintf(service_request, sizeof(service_request), "initialize");
    } else if (strcmp(path, "/tools") == 0 || strcmp(path, "/list_tools") == 0) {
        snprintf(service_request, sizeof(service_request), "list_tools");
    } else if (strcmp(path, "/call") == 0) {
        if (query == NULL) {
            snprintf(service_request, sizeof(service_request), "call_tool");
        } else {
            build_call_request_from_query(service_request,
                                          sizeof(service_request),
                                          query);
        }
    } else {
        write_error_body(body, sizeof(body), "unknown HTTP endpoint");
        send_http_response(fd, body);
        fclose(in);
        close(fd);
        return;
    }

    handle_request_line(body, sizeof(body), config, service_request);
    send_http_response(fd, body);
    fclose(in);
    close(fd);
}

static int load_config_file(struct serve_config *config, const char *path,
                            char *allow_storage, size_t allow_storage_size)
{
    FILE *file = fopen(path, "r");
    char line[512];

    if (file == NULL) {
        fprintf(stderr, "serve: %s: %s\n", path, strerror(errno));
        return 1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char *equals;
        char *key;
        char *value;

        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        equals = strchr(line, '=');
        if (equals == NULL) {
            fprintf(stderr, "serve: ignoring malformed config line: %s\n", line);
            continue;
        }

        *equals = '\0';
        key = line;
        value = equals + 1;
        trim_line(key);
        trim_line(value);

        if (strcmp(key, "host") == 0) {
            snprintf(config->host, sizeof(config->host), "%s", value);
        } else if (strcmp(key, "port") == 0) {
            if (!parse_int_range(value, 1, 65535, &config->port)) {
                fprintf(stderr, "serve: invalid port in config\n");
                fclose(file);
                return 1;
            }
        } else if (strcmp(key, "timeout") == 0) {
            if (!parse_int_range(value, 1, 300, &config->timeout_seconds)) {
                fprintf(stderr, "serve: invalid timeout in config\n");
                fclose(file);
                return 1;
            }
        } else if (strcmp(key, "max_requests") == 0) {
            if (!parse_int_range(value, 1, 1000, &config->max_requests)) {
                fprintf(stderr, "serve: invalid max_requests in config\n");
                fclose(file);
                return 1;
            }
        } else if (strcmp(key, "allow") == 0) {
            snprintf(allow_storage, allow_storage_size, "%s", value);
            config_set_allowed_csv(config, allow_storage);
        }
    }

    fclose(file);
    return 0;
}

static int create_server_socket(const struct serve_config *config)
{
    int fd;
    int yes = 1;
    struct sockaddr_in address;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "serve: socket: %s\n", strerror(errno));
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t) config->port);
    if (strcmp(config->host, "0.0.0.0") == 0) {
        address.sin_addr.s_addr = INADDR_ANY;
    } else if (inet_pton(AF_INET, config->host, &address.sin_addr) != 1) {
        fprintf(stderr, "serve: host must be an IPv4 address\n");
        close(fd);
        return -1;
    }

    if (bind(fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
        fprintf(stderr, "serve: bind %s:%d: %s\n",
                config->host,
                config->port,
                strerror(errno));
        close(fd);
        return -1;
    }

    if (listen(fd, SERVE_BACKLOG) < 0) {
        fprintf(stderr, "serve: listen: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

int serve_run(int argc, char **argv)
{
    struct serve_config config;
    char allow_storage[512] = "";
    int index;
    int server_fd;

    snprintf(config.host, sizeof(config.host), "%s", SERVE_DEFAULT_HOST);
    config.port = SERVE_DEFAULT_PORT;
    config.timeout_seconds = SERVE_DEFAULT_TIMEOUT_SECONDS;
    config.max_requests = SERVE_DEFAULT_MAX_REQUESTS;
    config.http_mode = 0;
    config.once = 0;
    config.allowed_count = 0;
    for (index = 0; index < (int) (sizeof(default_allowed) / sizeof(default_allowed[0]));
         index++) {
        config_add_allowed(&config, default_allowed[index]);
    }

    signal(SIGCHLD, SIG_IGN);

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            serve_print_usage(stdout);
            return 0;
        } else if (strcmp(argv[index], "--http") == 0) {
            config.http_mode = 1;
        } else if (strcmp(argv[index], "--once") == 0) {
            config.once = 1;
        } else if (strcmp(argv[index], "--host") == 0 && index + 1 < argc) {
            snprintf(config.host, sizeof(config.host), "%s", argv[++index]);
        } else if (strcmp(argv[index], "--port") == 0 && index + 1 < argc) {
            if (!parse_int_range(argv[++index], 1, 65535, &config.port)) {
                fprintf(stderr, "serve: port must be 1..65535\n");
                return 1;
            }
        } else if (strcmp(argv[index], "--timeout") == 0 && index + 1 < argc) {
            if (!parse_int_range(argv[++index], 1, 300, &config.timeout_seconds)) {
                fprintf(stderr, "serve: timeout must be 1..300 seconds\n");
                return 1;
            }
        } else if (strcmp(argv[index], "--max-requests") == 0 && index + 1 < argc) {
            if (!parse_int_range(argv[++index], 1, 1000, &config.max_requests)) {
                fprintf(stderr, "serve: max requests must be 1..1000\n");
                return 1;
            }
        } else if (strcmp(argv[index], "--allow") == 0 && index + 1 < argc) {
            snprintf(allow_storage, sizeof(allow_storage), "%s", argv[++index]);
            config_set_allowed_csv(&config, allow_storage);
        } else if (strcmp(argv[index], "--config") == 0 && index + 1 < argc) {
            if (load_config_file(&config, argv[++index],
                                 allow_storage, sizeof(allow_storage)) != 0) {
                return 1;
            }
        } else {
            fprintf(stderr, "serve: unknown option: %s\n", argv[index]);
            serve_print_usage(stderr);
            return 1;
        }
    }

    server_fd = create_server_socket(&config);
    if (server_fd < 0) {
        return 1;
    }

    printf("serve: listening on %s:%d (%s, timeout=%ds, max_requests=%d)\n",
           config.host,
           config.port,
           config.http_mode ? "http" : "line-rpc",
           config.timeout_seconds,
           config.max_requests);
    fflush(stdout);

    while (1) {
        int client_fd;
        pid_t pid;

        client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "serve: accept: %s\n", strerror(errno));
            close(server_fd);
            return 1;
        }

        if (config.once) {
            if (config.http_mode) {
                handle_http_client(client_fd, &config);
            } else {
                handle_client(client_fd, &config);
            }
            close(server_fd);
            return 0;
        }

        pid = fork();
        if (pid < 0) {
            fprintf(stderr, "serve: fork: %s\n", strerror(errno));
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            close(server_fd);
            if (config.http_mode) {
                handle_http_client(client_fd, &config);
            } else {
                handle_client(client_fd, &config);
            }
            _exit(0);
        }

        close(client_fd);
    }
}

void serve_print_usage(FILE *out)
{
    fprintf(out, "Usage: serve [--http] [--once] [--host HOST] [--port PORT] [--timeout SECONDS]\n");
    fprintf(out, "             [--max-requests COUNT] [--allow CSV] [--config FILE]\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Start the Session 9 AiShell socket service endpoint.\n");
    fprintf(out, "  Requests are one line: initialize, list_tools, or call_tool TOOL [ARGS...].\n");
    fprintf(out, "  With --http, use GET /initialize, /tools, or /call?tool=TOOL&arg=ARG.\n");
    fprintf(out, "  Arguments use simple tokens or key:value tokens such as path:.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-24s %s\n", "--http", "serve simple HTTP endpoints for curl and agents");
    fprintf(out, "  %-24s %s\n", "--once", "handle one client and exit, useful for tests and demos");
    fprintf(out, "  %-24s %s\n", "--host HOST", "IPv4 bind address, default 0.0.0.0");
    fprintf(out, "  %-24s %s\n", "--port PORT", "TCP port, default 9000");
    fprintf(out, "  %-24s %s\n", "--timeout SECONDS", "client read/write timeout, default 5");
    fprintf(out, "  %-24s %s\n", "--max-requests COUNT", "requests per connection, default 8");
    fprintf(out, "  %-24s %s\n", "--allow CSV", "enabled tools, default includes get_time,list_files,delete_older_than_days,http_get");
    fprintf(out, "  %-24s %s\n", "--config FILE", "key=value config file: host, port, timeout, max_requests, allow");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  serve --port 9000 --allow ls,pwd,mkdir\n");
    fprintf(out, "  rpc initialize\n");
    fprintf(out, "  rpc list_tools\n");
    fprintf(out, "  rpc call_tool list_files path:.\n");
    fprintf(out, "  rpc call_tool pwd\n");
    fprintf(out, "  serve --http --port 9000 --allow get_time,list_files,pwd\n");
    fprintf(out, "  curl http://127.0.0.1:9000/tools\n");
    fprintf(out, "  curl 'http://127.0.0.1:9000/call?tool=pwd'\n");
    fprintf(out, "  curl 'http://127.0.0.1:9000/call?tool=list_files&arg=path:.'\n");
    fprintf(out, "  curl 'http://127.0.0.1:9000/call?tool=delete_older_than_days&arg=path:.&arg=days:30'\n");
    fprintf(out, "  curl 'http://127.0.0.1:9000/call?tool=delete_older_than_days&arg=path:.&arg=days:30&arg=dry_run:false&arg=confirm:DELETE'\n");
    fprintf(out, "  curl 'http://127.0.0.1:9000/call?tool=http_get&arg=url:https://example.com&arg=limit:2048'\n");
}

cmd_spec_t cmd_serve_spec = {
    .name = "serve",
    .summary = "run the Session 9 socket service endpoint",
    .long_help = "Run a line-based TCP server for allowlisted shell services.",
    .run = serve_run,
    .print_usage = serve_print_usage,
};

void register_serve_command(void)
{
    register_command(&cmd_serve_spec);
}
