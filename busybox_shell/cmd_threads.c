#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cmd_spec.h"
#include "cmd_threads.h"

#define THREADS_DEFAULT_COUNT 4
#define THREADS_MAX_COUNT 64
#define THREADS_RACE_ITERATIONS 10
#define THREADS_SIGNAL_DEFAULT_TICKS 12

struct thread_job {
    int index;
    unsigned long result;
    unsigned long thread_id;
    unsigned int sleep_ms;
    unsigned long *shared_sum;
    pthread_mutex_t *sum_lock;
    unsigned int ticks;
    int verbose;
};

enum thread_demo_mode {
    THREAD_MODE_SUM,
    THREAD_MODE_COUNTDOWN,
    THREAD_MODE_RACE,
    THREAD_MODE_MUTEX,
    THREAD_MODE_SIGNAL,
};

struct counter_job {
    int index;
    unsigned long thread_id;
    long *counter;
    pthread_mutex_t *counter_lock;
    int use_mutex;
};

static volatile sig_atomic_t signal_demo_stop;

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

static const char *mode_name(enum thread_demo_mode mode)
{
    switch (mode) {
    case THREAD_MODE_SUM:
        return "sum";
    case THREAD_MODE_COUNTDOWN:
        return "countdown";
    case THREAD_MODE_RACE:
        return "race";
    case THREAD_MODE_MUTEX:
        return "mutex";
    case THREAD_MODE_SIGNAL:
        return "signal";
    }
    return "sum";
}

static int parse_mode(const char *text, enum thread_demo_mode *mode)
{
    if (strcmp(text, "sum") == 0 || strcmp(text, "squares") == 0) {
        *mode = THREAD_MODE_SUM;
        return 1;
    }
    if (strcmp(text, "countdown") == 0) {
        *mode = THREAD_MODE_COUNTDOWN;
        return 1;
    }
    if (strcmp(text, "race") == 0) {
        *mode = THREAD_MODE_RACE;
        return 1;
    }
    if (strcmp(text, "mutex") == 0) {
        *mode = THREAD_MODE_MUTEX;
        return 1;
    }
    if (strcmp(text, "signal") == 0) {
        *mode = THREAD_MODE_SIGNAL;
        return 1;
    }
    return 0;
}

static void signal_demo_handler(int signo)
{
    (void) signo;
    signal_demo_stop = 1;
}

static void *countdown_worker(void *arg)
{
    struct thread_job *job = arg;
    int step;
    struct timespec delay;

    job->thread_id = (unsigned long)pthread_self();
    delay.tv_sec = 0;
    delay.tv_nsec = 10000000L;

    for (step = 3; step >= 1; step--) {
        printf("thread %d id %lu countdown %d\n",
               job->index + 1,
               job->thread_id,
               step);
        nanosleep(&delay, NULL);
    }
    job->result = (unsigned long)(job->index + 1);
    return NULL;
}

static void *counter_worker(void *arg)
{
    struct counter_job *job = arg;
    int iteration;
    long before;
    long after;

    job->thread_id = (unsigned long)pthread_self();
    for (iteration = 0; iteration < THREADS_RACE_ITERATIONS; iteration++) {
        if (job->use_mutex) {
            pthread_mutex_lock(job->counter_lock);
            before = *job->counter;
            (*job->counter)++;
            after = *job->counter;
            printf("thread %d id %lu locked counter: %ld -> %ld\n",
                   job->index + 1,
                   job->thread_id,
                   before,
                   after);
            pthread_mutex_unlock(job->counter_lock);
        } else {
            before = *job->counter;
            nanosleep(&(struct timespec){0, 1000000L}, NULL);
            *job->counter = before + 1;
            after = *job->counter;
            printf("thread %d id %lu race counter: %ld -> %ld\n",
                   job->index + 1,
                   job->thread_id,
                   before,
                   after);
        }
    }
    return NULL;
}

static void *signal_worker(void *arg)
{
    struct thread_job *job = arg;
    struct timespec delay;
    int heartbeat = 0;

    job->thread_id = (unsigned long)pthread_self();
    delay.tv_sec = 0;
    delay.tv_nsec = 250000000L;

    printf("thread %d id %lu started\n", job->index + 1, job->thread_id);
    fflush(stdout);

    while (!signal_demo_stop && heartbeat < (int)job->ticks) {
        heartbeat++;
        if (job->verbose) {
            printf("thread %d id %lu running %d\n",
                   job->index + 1,
                   job->thread_id,
                   heartbeat);
            fflush(stdout);
        }
        nanosleep(&delay, NULL);
    }

    if (signal_demo_stop) {
        printf("thread %d id %lu stopping after SIGTERM\n",
               job->index + 1,
               job->thread_id);
    } else {
        printf("thread %d id %lu completed %u heartbeats\n",
               job->index + 1,
               job->thread_id,
               job->ticks);
    }
    fflush(stdout);
    return NULL;
}

static int run_countdown_demo(int count, int json)
{
    pthread_t threads[THREADS_MAX_COUNT];
    struct thread_job jobs[THREADS_MAX_COUNT];
    int index;

    for (index = 0; index < count; index++) {
        jobs[index].index = index;
        jobs[index].result = 0;
        jobs[index].thread_id = 0;
        jobs[index].sleep_ms = 0;
        jobs[index].shared_sum = NULL;
        jobs[index].sum_lock = NULL;
        if (pthread_create(&threads[index], NULL, countdown_worker, &jobs[index]) != 0) {
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
        printf("{\"mode\":\"countdown\",\"threads\":%d,\"complete\":true}\n", count);
    } else {
        printf("countdown complete with %d threads\n", count);
    }
    return ferror(stdout) ? 1 : 0;
}

static int run_counter_demo(int count, int json, int use_mutex)
{
    pthread_t threads[THREADS_MAX_COUNT];
    struct counter_job jobs[THREADS_MAX_COUNT];
    pthread_mutex_t counter_lock = PTHREAD_MUTEX_INITIALIZER;
    long counter = 0;
    long expected = (long) count * THREADS_RACE_ITERATIONS;
    int index;

    for (index = 0; index < count; index++) {
        jobs[index].index = index;
        jobs[index].thread_id = 0;
        jobs[index].counter = &counter;
        jobs[index].counter_lock = &counter_lock;
        jobs[index].use_mutex = use_mutex;
        if (pthread_create(&threads[index], NULL, counter_worker, &jobs[index]) != 0) {
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
        printf("{\"mode\":\"%s\",\"threads\":%d,\"iterations_per_thread\":%d,"
               "\"expected\":%ld,\"actual\":%ld,\"protected\":%s}\n",
               use_mutex ? "mutex" : "race",
               count,
               THREADS_RACE_ITERATIONS,
               expected,
               counter,
               use_mutex ? "true" : "false");
    } else {
        printf("mode %s\n", use_mutex ? "mutex" : "race");
        printf("threads %d iterations_per_thread %d\n", count, THREADS_RACE_ITERATIONS);
        printf("expected counter %ld\n", expected);
        printf("actual counter %ld\n", counter);
        if (use_mutex) {
            printf("mutex protected shared counter\n");
        } else {
            printf("race mode has no mutex; result may lose updates\n");
        }
    }

    return ferror(stdout) ? 1 : 0;
}

static int run_signal_demo(int count, int json, int ticks, int verbose)
{
    pthread_t threads[THREADS_MAX_COUNT];
    struct thread_job jobs[THREADS_MAX_COUNT];
    struct sigaction action;
    int index;

    signal_demo_stop = 0;
    memset(&action, 0, sizeof(action));
    action.sa_handler = signal_demo_handler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGTERM, &action, NULL) != 0 ||
        sigaction(SIGINT, &action, NULL) != 0) {
        fprintf(stderr, "threads: sigaction: %s\n", strerror(errno));
        return 1;
    }

    if (json) {
        printf("{\"mode\":\"signal\",\"pid\":%lu,\"threads\":%d,\"status\":\"started\"}\n",
               (unsigned long)getpid(),
               count);
    } else {
        printf("signal demo pid %lu started %d threads for %d heartbeats\n",
               (unsigned long)getpid(),
               count,
               ticks);
        printf("send SIGTERM early with: kill -15 %%JOB\n");
    }
    fflush(stdout);

    for (index = 0; index < count; index++) {
        jobs[index].index = index;
        jobs[index].result = 0;
        jobs[index].thread_id = 0;
        jobs[index].sleep_ms = 0;
        jobs[index].shared_sum = NULL;
        jobs[index].sum_lock = NULL;
        jobs[index].ticks = (unsigned int)ticks;
        jobs[index].verbose = verbose;
        if (pthread_create(&threads[index], NULL, signal_worker, &jobs[index]) != 0) {
            fprintf(stderr, "threads: pthread_create failed for worker %d\n", index + 1);
            signal_demo_stop = 1;
            return 1;
        }
    }

    for (index = 0; index < count; index++) {
        if (pthread_join(threads[index], NULL) != 0) {
            fprintf(stderr, "threads: pthread_join failed for worker %d\n", index + 1);
            return 1;
        }
    }

    if (signal_demo_stop) {
        printf("SIGTERM received by process %lu\n", (unsigned long)getpid());
    } else {
        printf("signal demo completed without signal\n");
    }
    fflush(stdout);

    if (json) {
        printf("{\"mode\":\"signal\",\"pid\":%lu,\"threads\":%d,\"status\":\"stopped\"}\n",
               (unsigned long)getpid(),
               count);
    } else {
        printf("all signal demo threads stopped\n");
    }
    return ferror(stdout) ? 1 : 0;
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
    int ticks = THREADS_SIGNAL_DEFAULT_TICKS;
    int verbose = 0;
    enum thread_demo_mode mode = THREAD_MODE_SUM;

    for (index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-h") == 0 || strcmp(argv[index], "--help") == 0) {
            threads_print_usage(stdout);
            return 0;
        } else if (strcmp(argv[index], "--json") == 0) {
            json = 1;
        } else if (strcmp(argv[index], "-v") == 0 || strcmp(argv[index], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[index], "-n") == 0 || strcmp(argv[index], "--count") == 0) {
            if (index + 1 >= argc ||
                !parse_int_range(argv[index + 1], 1, THREADS_MAX_COUNT, &count)) {
                fprintf(stderr, "threads: expected count from 1 to %d\n", THREADS_MAX_COUNT);
                return 1;
            }
            index++;
        } else if (strcmp(argv[index], "--mode") == 0) {
            if (index + 1 >= argc || !parse_mode(argv[index + 1], &mode)) {
                fprintf(stderr, "threads: expected mode sum, countdown, race, mutex, or signal\n");
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
        } else if (strcmp(argv[index], "--ticks") == 0) {
            if (index + 1 >= argc ||
                !parse_int_range(argv[index + 1], 1, 1000, &ticks)) {
                fprintf(stderr, "threads: expected tick count from 1 to 1000\n");
                return 1;
            }
            index++;
        } else {
            fprintf(stderr, "threads: unknown option: %s\n", argv[index]);
            threads_print_usage(stderr);
            return 1;
        }
    }

    if (mode == THREAD_MODE_COUNTDOWN) {
        return run_countdown_demo(count, json);
    }
    if (mode == THREAD_MODE_RACE) {
        return run_counter_demo(count, json, 0);
    }
    if (mode == THREAD_MODE_MUTEX) {
        return run_counter_demo(count, json, 1);
    }
    if (mode == THREAD_MODE_SIGNAL) {
        return run_signal_demo(count, json, ticks, verbose);
    }

    for (index = 0; index < count; index++) {
        jobs[index].index = index;
        jobs[index].result = 0;
        jobs[index].thread_id = 0;
        jobs[index].sleep_ms = (unsigned int)sleep_ms;
        jobs[index].shared_sum = &shared_sum;
        jobs[index].sum_lock = &sum_lock;
        jobs[index].verbose = verbose;
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
        printf("{\"mode\":\"%s\",\"threads\":%d,\"sleep_ms\":%d,\"sum\":%lu,\"results\":[",
               mode_name(mode),
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
    fprintf(out, "Usage: threads [-n COUNT] [--mode MODE] [--sleep MS] [--ticks COUNT] [--json] [--verbose]\n");

    fprintf(out, "\nDescription:\n");
    fprintf(out, "  Start POSIX worker threads, join them, and print demo results.\n");

    fprintf(out, "\nOptions:\n");
    fprintf(out, "  %-20s %s\n", "-h, --help", "show help and exit");
    fprintf(out, "  %-20s %s\n", "-n, --count COUNT", "number of worker threads, 1 to 64");
    fprintf(out, "  %-20s %s\n", "--mode MODE", "sum, countdown, race, mutex, or signal");
    fprintf(out, "  %-20s %s\n", "--sleep MS", "milliseconds each worker sleeps before saving its result");
    fprintf(out, "  %-20s %s\n", "--ticks COUNT", "heartbeats for signal mode before auto-stop");
    fprintf(out, "  %-20s %s\n", "--json", "output in JSON format");
    fprintf(out, "  %-20s %s\n", "-v, --verbose", "show every signal-mode heartbeat");
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
