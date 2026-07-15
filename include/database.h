#ifndef DATABASE_H
#define DATABASE_H

#include <stddef.h>
#include <sqlite3.h>

#include "utils.h"

typedef struct {
    int account_no;
    char name[ATM_NAME_LENGTH];
    double balance;
    int is_locked;
    int failed_attempts;
} AccountInfo;

typedef struct {
    sqlite3 *conn;
} Database;

int db_open(Database *db, const char *db_path, char *error, size_t error_size);
void db_close(Database *db);

int db_read_account(Database *db, int account_no, AccountInfo *info,
                    unsigned long long *pin_hash, unsigned long long *salt,
                    int *found, char *error, size_t error_size);
int db_update_login_state(Database *db, int account_no,
                          int failed_attempts, int is_locked,
                          char *error, size_t error_size);
int db_update_balance(Database *db, int account_no, double balance,
                      char *error, size_t error_size);
int db_insert_transaction(Database *db, int account_no,
                          int has_related, int related_account,
                          const char *type, double amount,
                          int has_balance_after, double balance_after,
                          const char *timestamp,
                          char *error, size_t error_size);

int db_begin_transaction(Database *db, char *error, size_t error_size);
int db_commit_transaction(Database *db, char *error, size_t error_size);
void db_rollback_transaction(Database *db);

int db_exec_sql(Database *db, const char *sql,
                char *error, size_t error_size);
int db_count_rows(Database *db, const char *table_name, int *count,
                  char *error, size_t error_size);

void set_error(char *error, size_t error_size, const char *fmt, ...);
int append_formatted(char *buffer, size_t buffer_size, size_t *used,
                     const char *fmt, ...);
const char *display_type(const char *type);

#endif
