#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

void log_message(LogLevel level, const char *format, ...);

#define log_info(...)  log_message(LOG_LEVEL_INFO, __VA_ARGS__)
#define log_warn(...)  log_message(LOG_LEVEL_WARN, __VA_ARGS__)
#define log_error(...) log_message(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif
