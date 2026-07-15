#include "utils.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void discard_remaining_stdin_line(void) {
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
    }
}

void trim_newline(char *text) {
    size_t length;

    if (text == NULL) {
        return;
    }

    length = strlen(text);
    while (length > 0 && (text[length - 1] == '\n' || text[length - 1] == '\r')) {
        text[length - 1] = '\0';
        length--;
    }
}

char *trim_whitespace(char *text) {
    char *start = text;
    char *end;

    if (text == NULL) {
        return NULL;
    }

    while (*start != '\0' && isspace((unsigned char) *start)) {
        start++;
    }

    end = start + strlen(start);
    while (end > start && isspace((unsigned char) end[-1])) {
        end--;
    }

    *end = '\0';
    return start;
}

int parse_int(const char *text, int *value) {
    char *end = NULL;
    long parsed;

    if (text == NULL || value == NULL) {
        return 0;
    }

    parsed = strtol(text, &end, 10);
    if (end == text) {
        return 0;
    }

    while (*end != '\0' && isspace((unsigned char) *end)) {
        end++;
    }

    if (*end != '\0' || parsed < INT_MIN || parsed > INT_MAX) {
        return 0;
    }

    *value = (int) parsed;
    return 1;
}

int parse_double(const char *text, double *value) {
    char *end = NULL;
    double parsed;

    if (text == NULL || value == NULL) {
        return 0;
    }

    parsed = strtod(text, &end);
    if (end == text) {
        return 0;
    }

    while (*end != '\0' && isspace((unsigned char) *end)) {
        end++;
    }

    if (*end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

int read_line_stdin(const char *prompt, char *buffer, size_t buffer_size) {
    size_t length;

    if (prompt != NULL) {
        printf("%s", prompt);
    }

    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    if (fgets(buffer, (int) buffer_size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0U && buffer[length - 1] != '\n') {
        discard_remaining_stdin_line();
    }

    trim_newline(buffer);
    return 1;
}

int read_required_text_stdin(const char *prompt, char *buffer, size_t buffer_size) {
    char *trimmed;

    if (!read_line_stdin(prompt, buffer, buffer_size)) {
        return 0;
    }

    trimmed = trim_whitespace(buffer);
    if (*trimmed == '\0') {
        return 0;
    }

    if (trimmed != buffer) {
        memmove(buffer, trimmed, strlen(trimmed) + 1U);
    }

    return 1;
}

int read_int_stdin(const char *prompt, int *value) {
    char buffer[128];

    if (!read_line_stdin(prompt, buffer, sizeof(buffer))) {
        return 0;
    }

    return parse_int(buffer, value);
}

int read_double_stdin(const char *prompt, double *value) {
    char buffer[128];

    if (!read_line_stdin(prompt, buffer, sizeof(buffer))) {
        return 0;
    }

    return parse_double(buffer, value);
}

void current_timestamp(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm tm_info;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    now = time(NULL);
    if (localtime_r(&now, &tm_info) == NULL ||
        strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", &tm_info) == 0) {
        snprintf(buffer, buffer_size, "1970-01-01 00:00:00");
    }
}

void current_year_month(char *buffer, size_t buffer_size) {
    time_t now;
    struct tm tm_info;

    if (buffer == NULL || buffer_size == 0) {
        return;
    }

    now = time(NULL);
    if (localtime_r(&now, &tm_info) == NULL ||
        strftime(buffer, buffer_size, "%Y-%m", &tm_info) == 0) {
        snprintf(buffer, buffer_size, "1970-01");
    }
}
