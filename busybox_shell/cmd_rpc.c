#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "cmd_rpc.h"
#include "cmd_spec.h"
#include "json_utils.h"

#define RPC_DEFAULT_HOST "127.0.0.1"
#define RPC_DEFAULT_PORT "9000"
#define RPC_DEFAULT_TIMEOUT_SECONDS 5
#define RPC_REQUEST_SIZE 2048
#define RPC_RESPONSE_SIZE 4096

static void rpc_print_protocol(FILE *out)
{
    fprintf(out, "Line-based RPC protocol:\n");
    fprintf(out, "  Each request is one UTF-8 text line ending with newline.\n");
    fprintf(out, "  Format: METHOD [ARGS...]\n");
    fprintf(out, "  Arguments are space-separated tokens.\n");
    fprintf(out, "  Key/value arguments use colon syntax: key:value\n");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  initialize\n");
    fprintf(out, "  list_tools\n");
    fprintf(out, "  call_tool get_time\n");
    fprintf(out, "  call_tool list_files path:.\n");
}

static int rpc_connect(const char *host, const char *port)
{
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *item;
    int fd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    rc = getaddrinfo(host, port, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "rpc: getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    for (item = result; item != NULL; item = item->ai_next) {
        fd = socket(item->ai_family, item->ai_socktype, item->ai_protocol);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, item->ai_addr, item->ai_addrlen) == 0) {
            break;
        }
        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

static int rpc_build_request(int argc, char **argv, int start,
                             char *request, size_t request_size)
{
    size_t used = 0;
    int index;

    request[0] = '\0';
    for (index = start; index < argc; index++) {
        int written = snprintf(request + used, request_size - used, "%s%s",
                               index == start ? "" : " ", argv[index]);

        if (written < 0 || (size_t) written >= request_size - used) {
            fprintf(stderr, "rpc: request too long\n");
            return 1;
        }
        used += (size_t) written;
    }

    if (used + 2 >= request_size) {
        fprintf(stderr, "rpc: request too long\n");
        return 1;
    }

    request[used++] = '\n';
    request[used] = '\0';
    return 0;
}

static int rpc_set_timeouts(int fd, int timeout_seconds)
{
    struct timeval timeout;

    timeout.tv_sec = timeout_seconds;
    timeout.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "rpc: receive timeout: %s\n", strerror(errno));
        return 1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO,
                   &timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "rpc: send timeout: %s\n", strerror(errno));
        return 1;
    }

    return 0;
}

static int rpc_parse_timeout(const char *text, int *timeout_seconds)
{
    char *endptr;
    long parsed;

    errno = 0;
    parsed = strtol(text, &endptr, 10);
    if (errno != 0 || endptr == text || *endptr != '\0' ||
        parsed <= 0 || parsed > 300) {
        fprintf(stderr, "rpc: timeout must be an integer from 1 to 300 seconds\n");
        return 1;
    }

    *timeout_seconds = (int) parsed;
    return 0;
}

static int rpc_send_request(const char *host, const char *port,
                            int timeout_seconds,
                            const char *request, int json)
{
    char response[RPC_RESPONSE_SIZE];
    int fd;
    ssize_t count;
    int got_response = 0;
    int complete_line = 0;

    fd = rpc_connect(host, port);
    if (fd < 0) {
        fprintf(stderr, "rpc: could not connect to %s:%s\n", host, port);
        return 1;
    }

    if (rpc_set_timeouts(fd, timeout_seconds) != 0) {
        close(fd);
        return 1;
    }

    if (write(fd, request, strlen(request)) < 0) {
        fprintf(stderr, "rpc: write: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    if (json) {
        printf("{\"host\":");
        json_print_string(stdout, host);
        printf(",\"port\":");
        json_print_string(stdout, port);
        printf(",\"timeout_seconds\":%d", timeout_seconds);
        printf(",\"response\":\"");
    }

    while ((count = read(fd, response, sizeof(response) - 1)) > 0) {
        response[count] = '\0';
        got_response = 1;
        if (strchr(response, '\n') != NULL) {
            complete_line = 1;
        }
        if (json) {
            char *cursor = response;

            while (*cursor != '\0') {
                json_print_escaped_char(stdout, *cursor);
                cursor++;
            }
        } else {
            fputs(response, stdout);
        }
        if (complete_line) {
            break;
        }
    }

    if (json) {
        printf("\"}\n");
    }

    if (count < 0 &&
        !(got_response && (errno == EAGAIN || errno == EWOULDBLOCK))) {
        fprintf(stderr, "rpc: read: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}

int rpc_run(int argc, char **argv)
{
    const char *host = RPC_DEFAULT_HOST;
    const char *port = RPC_DEFAULT_PORT;
    char request[RPC_REQUEST_SIZE];
    int timeout_seconds = RPC_DEFAULT_TIMEOUT_SECONDS;
    int json = 0;
    int arg_index = 1;

    while (arg_index < argc) {
        if (strcmp(argv[arg_index], "--host") == 0 && arg_index + 1 < argc) {
            host = argv[arg_index + 1];
            arg_index += 2;
        } else if (strcmp(argv[arg_index], "--port") == 0 && arg_index + 1 < argc) {
            port = argv[arg_index + 1];
            arg_index += 2;
        } else if (strcmp(argv[arg_index], "--timeout") == 0 && arg_index + 1 < argc) {
            if (rpc_parse_timeout(argv[arg_index + 1], &timeout_seconds) != 0) {
                return 1;
            }
            arg_index += 2;
        } else if (strcmp(argv[arg_index], "--json") == 0) {
            json = 1;
            arg_index++;
        } else if (strcmp(argv[arg_index], "--protocol") == 0) {
            rpc_print_protocol(stdout);
            return 0;
        } else if (strcmp(argv[arg_index], "-h") == 0 ||
                   strcmp(argv[arg_index], "--help") == 0) {
            rpc_print_usage(stdout);
            return 0;
        } else {
            break;
        }
    }

    if (arg_index >= argc) {
        rpc_print_usage(stderr);
        return 1;
    }

    if (rpc_build_request(argc, argv, arg_index, request, sizeof(request)) != 0) {
        return 1;
    }

    return rpc_send_request(host, port, timeout_seconds, request, json);
}

void rpc_print_usage(FILE *out)
{
    fprintf(out, "Usage: rpc [--host HOST] [--port PORT] [--timeout SECONDS] [--json] REQUEST...\n");
    fprintf(out, "       rpc --protocol\n");
    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Send one line-based RPC request to a local or remote service.\n");
    fprintf(out, "  This is the Session 8 client-facing command for MCP-style demos.\n");
    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "--host HOST", "server host, default 127.0.0.1");
    fprintf(out, "  %-20s %s\n", "--port PORT", "server port, default 9000");
    fprintf(out, "  %-20s %s\n", "--timeout SECONDS", "send/receive timeout, default 5 seconds");
    fprintf(out, "  %-20s %s\n", "--json", "wrap the response in JSON output");
    fprintf(out, "  %-20s %s\n", "--protocol", "print the line-based protocol");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "\nExamples:\n");
    fprintf(out, "  rpc initialize\n");
    fprintf(out, "  rpc list_tools\n");
    fprintf(out, "  rpc call_tool get_time\n");
    fprintf(out, "  rpc call_tool list_files path:.\n");
}

cmd_spec_t cmd_rpc_spec = {
    .name = "rpc",
    .summary = "send a line-based RPC request",
    .long_help = "Send one line-based RPC request to a TCP service.",
    .run = rpc_run,
    .print_usage = rpc_print_usage,
};

void register_rpc_command(void)
{
    register_command(&cmd_rpc_spec);
}
