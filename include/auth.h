#ifndef AUTH_H
#define AUTH_H

#include <stddef.h>
#include "database.h"

int db_create_account(Database *db, int account_no, const char *name,
                      int pin, char *error, size_t error_size);
int db_login(Database *db, int account_no, int pin, AccountInfo *info,
             char *error, size_t error_size);
int db_admin_login(Database *db, const char *username, const char *password,
                   char *error, size_t error_size);
int db_change_admin_password(Database *db, const char *username,
                             const char *new_password,
                             char *error, size_t error_size);
int db_seed_admin(Database *db, char *error, size_t error_size);

#endif
