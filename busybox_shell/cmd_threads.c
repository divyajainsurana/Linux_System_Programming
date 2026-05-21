#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cmd_spec.h"
#include "cmd_threads.h"

#define THREADS_DEFAULT_COUNT 4
#define THREADS_MAX_COUNT 64

struct thread_job {
    int index;
    unsigned long result;
    unsigned long thread_id;
    unsigned int sleep_ms;
    unsigned long *shared_sum;
    pthread_mutex_t *sum_lock;
};

static void *thread_worker(void *arg)
{
    struct thread_job *job = arg;
    unsigned long value = (unsigned long) (job->index + 1);
    struct timespec delay;

    job->thread_id = (unsigned long)pthread_self();
    if (job->sleep_ms > 0) {
        delay.tv_sec = job->sleep_ms / 1000;
        delay.tv_nsec = (long)(job->sleep_ms % 1000) * 1000000L;
        nanosleep(&delay, NULL);
    }
    job->result = value * value;

    pthread_mutex_lock(job->sum_lock);
    *job->shared_sum += job->result;
    pthread_mutex_unlock(job->sum_lock);

    return NULL;
}

static int parse_int_range(const char *text, int min_value, int max_value, int *result)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        value < min_value || value > max_value) {
        return 0;
    }

    *result = (int) value;
    return 1;
}

int threads_run(int argc, char **argv)
{
    pthread_t threads[THREADS_MAX_COUNT];
    struct thread_job jobs[THREADS_MAX_COUNT];
    pthread_mutex_t sum_lock = PTHREAD_MUTEX_INITIALIZER;
    unsigned long shared_sum = 0;
    int count = THREADS_DEFAULT_COUNT;
    int sleep_ms = 0;
    int json = 0;
    int index;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            threads_print_usage(stdout);
            return 0;
        } else if (strcmp(argv[index], "--json") == 0) {
            json = 1;
        } else if (strcmp(argv[index], "-n") == 0 || strcmp(argv[index], "--count") == 0) {
            if (index + 1 >= argc ||
                !parse_int_range(argv[index + 1], 1, THREADS_MAX_COUNT, &count)) {
                fprintf(stderr, "threads: expected count from 1 to %d\n", THREADS_MAX_COUNT);
                return 1;
            }
            index++;
        } else if (strcmp(argv[index], "--sleep") == 0) {
            if (index + 1 >= argc ||
                !parse_int_range(argv[index + 1], 0, 60000, &sleep_ms)) {
                fprintf(stderr, "threads: expected sleep milliseconds from 0 to 60000\n");
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
        jobs[index].thread_id = 0;
        jobs[index].sleep_ms = (unsigned int)sleep_ms;
        jobs[index].shared_sum = &shared_sum;
        jobs[index].sum_lock = &sum_lock;
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
        printf("{\"threads\":%d,\"sleep_ms\":%d,\"sum\":%lu,\"results\":[",
               count,
               sleep_ms,
               shared_sum);
        for (index = 0; index < count; index++) {
            if (index > 0) {
                putchar(',');
            }
            printf("{\"worker\":%d,\"thread_id\":%lu,\"result\":%lu}",
                   jobs[index].index + 1,
                   jobs[index].thread_id,
                   jobs[index].result);
        }
        printf("]}\n");
    } else {
        printf("started %d threads\n", count);
        for (index = 0; index < count; index++) {
            printf("thread %d id %lu result %lu\n",
                   jobs[index].index + 1,
                   jobs[index].thread_id,
                   jobs[index].result);
        }
        printf("sum %lu\n", shared_sum);
    }

    return ferror(stdout) ? 1 : 0;
}

void threads_print_usage(FILE *out)
{
    fprintf(out, "Usage: threads [-n COUNT] [--sleep MS] [--json]\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Start POSIX worker threads, join them, and print their results.\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-n, --count COUNT", "number of worker threads, 1 to 64");
    fprintf(out, "  %-20s %s\n", "--sleep MS", "milliseconds each worker sleeps before saving its result");
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
