#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stddef.h>
#include "database.h"

int db_get_balance(Database *db, int account_no, double *balance,
                   char *error, size_t error_size);
int db_deposit(Database *db, int account_no, double amount,
               double *new_balance, char *error, size_t error_size);
int db_withdraw(Database *db, int account_no, double amount,
                double *new_balance, char *error, size_t error_size);
int db_transfer(Database *db, int from_account, int to_account,
                double amount, double *new_balance,
                char *error, size_t error_size);
int db_build_mini_statement(Database *db, int account_no, char *output,
                            size_t output_size, char *error, size_t error_size);
int db_build_monthly_summary(Database *db, int account_no, const char *month,
                             char *output, size_t output_size,
                             char *error, size_t error_size);
int db_lock_account(Database *db, int account_no,
                    char *error, size_t error_size);
int db_unlock_account(Database *db, int account_no,
                      char *error, size_t error_size);
int db_admin_credit(Database *db, int account_no, double amount,
                    double *new_balance, char *error, size_t error_size);
int db_admin_debit(Database *db, int account_no, double amount,
                   double *new_balance, char *error, size_t error_size);
int db_edit_account(Database *db, int current_account_no, int new_account_no,
                    const char *new_name, int new_pin,
                    char *error, size_t error_size);
int db_delete_account(Database *db, int account_no,
                      char *error, size_t error_size);
int db_build_account_listing(Database *db, char *output, size_t output_size,
                             char *error, size_t error_size);
int db_build_account_details(Database *db, int account_no, char *output,
                             size_t output_size, char *error, size_t error_size);
int db_build_system_summary(Database *db, char *output, size_t output_size,
                            char *error, size_t error_size);
int db_build_recent_transactions(Database *db, int limit, char *output,
                                 size_t output_size, char *error, size_t error_size);

#endif
