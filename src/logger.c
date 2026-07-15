#include "logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void log_message(LogLevel level, const char *format, ...) {
    char timestamp[32];
    time_t now;
    struct tm tm_info;
    const char *label;

    now = time(NULL);
    if (localtime_r(&now, &tm_info) == NULL ||
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info) == 0) {
        snprintf(timestamp, sizeof(timestamp), "1970-01-01 00:00:00");
    }

    switch (level) {
        case LOG_LEVEL_INFO:  label = "INFO";  break;
        case LOG_LEVEL_WARN:  label = "WARN";  break;
        case LOG_LEVEL_ERROR: label = "ERROR"; break;
        default:              label = "?";     break;
    }

    fprintf(stdout, "[%s] [%s] ", timestamp, label);

    {
        va_list args;
        va_start(args, format);
        vfprintf(stdout, format, args);
        va_end(args);
    }

    fprintf(stdout, "\n");
    fflush(stdout);
}
