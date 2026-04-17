#include <stdio.h>
#include <time.h>

int main() {
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
