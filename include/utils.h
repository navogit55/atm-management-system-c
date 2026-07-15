#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

#define ATM_DEFAULT_HOST "127.0.0.1"
#define ATM_DEFAULT_PORT 5555
#define ATM_MAX_MESSAGE_SIZE 32768
#define ATM_NAME_LENGTH 64
#define ATM_STATEMENT_LIMIT 5

void trim_newline(char *text);
char *trim_whitespace(char *text);

int parse_int(const char *text, int *value);
int parse_double(const char *text, double *value);

int read_line_stdin(const char *prompt, char *buffer, size_t buffer_size);
int read_required_text_stdin(const char *prompt, char *buffer, size_t buffer_size);
int read_int_stdin(const char *prompt, int *value);
int read_double_stdin(const char *prompt, double *value);

void current_timestamp(char *buffer, size_t buffer_size);
void current_year_month(char *buffer, size_t buffer_size);

#endif
