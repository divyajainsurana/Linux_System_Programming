#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd_spec.h"
#include "cmd_threads.h"

#define THREADS_DEFAULT_COUNT 4
#define THREADS_MAX_COUNT 64

struct thread_job {
    int index;
    unsigned long result;
};

static void *thread_worker(void *arg)
{
    struct thread_job *job = arg;
    unsigned long value = (unsigned long) (job->index + 1);

    job->result = value * value;
    return NULL;
}

static int parse_thread_count(const char *text, int *count)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value < 1 || value > THREADS_MAX_COUNT) {
        return 0;
    }

    *count = (int) value;
    return 1;
}

int threads_run(int argc, char **argv)
{
    pthread_t threads[THREADS_MAX_COUNT];
    struct thread_job jobs[THREADS_MAX_COUNT];
    int count = THREADS_DEFAULT_COUNT;
    int json = 0;
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            threads_print_usage(stdout);
            return 0;
        } else if (strcmp(argv[index], "--json") == 0) {
            json = 1;
        } else if (strcmp(argv[index], "-n") == 0 || strcmp(argv[index], "--count") == 0) {
            if (index + 1 >= argc || !parse_thread_count(argv[index + 1], &count)) {
                fprintf(stderr, "threads: expected count from 1 to %d\n", THREADS_MAX_COUNT);
                return 1;
            }
            index++;
        } else {
            fprintf(stderr, "threads: unknown option: %s\n", argv[index]);
            threads_print_usage(stderr);
            return 1;
        }
    }

    for (index = 0; index < count; index++) {
        jobs[index].index = index;
        jobs[index].result = 0;
        if (pthread_create(&threads[index], NULL, thread_worker, &jobs[index]) != 0) {
            fprintf(stderr, "threads: pthread_create failed for worker %d\n", index + 1);
            return 1;
        }
    }

    for (index = 0; index < count; index++) {
        if (pthread_join(threads[index], NULL) != 0) {
            fprintf(stderr, "threads: pthread_join failed for worker %d\n", index + 1);
            return 1;
        }
    }

    if (json) {
        printf("{\"threads\":%d,\"results\":[", count);
        for (index = 0; index < count; index++) {
            if (index > 0) {
                putchar(',');
            }
            printf("{\"worker\":%d,\"result\":%lu}", jobs[index].index + 1, jobs[index].result);
        }
        printf("]}\n");
    } else {
        printf("started %d threads\n", count);
        for (index = 0; index < count; index++) {
            printf("thread %d result %lu\n", jobs[index].index + 1, jobs[index].result);
        }
    }

    return ferror(stdout) ? 1 : 0;
}

void threads_print_usage(FILE *out)
{
    fprintf(out, "Usage: threads [-n COUNT] [--json]\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Start POSIX worker threads, join them, and print their results.\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-n, --count COUNT", "number of worker threads, 1 to 64");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
}

cmd_spec_t cmd_threads_spec = {
    .name = "threads",
    .summary = "run a POSIX threads demo",
    .long_help = "Start POSIX worker threads, join them, and print their results.",
    .run = threads_run,
    .print_usage = threads_print_usage,
};

void register_threads_command(void)
{
    register_command(&cmd_threads_spec);
}
