#include <stdio.h>
#include <time.h>
#include <string.h>

void print_help() {
    printf("Usage: mydate [options]\n");
    printf("Options:\n");
    printf("  -h    Show this help message\n");
}

int main(int argc, char *argv[]) {

    //Check for -h flag
    if (argc > 1 && strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }

    time_t t;
    struct tm *local;

    // Get current time
    time(&t);

    // Convert to local time
    local = localtime(&t);

    // Print date (YYYY-MM-DD format)
    printf("Local Date: %04d-%02d-%02d\n",
           local->tm_year + 1900,
           local->tm_mon + 1,
           local->tm_mday);

    return 0;
}
