#include "database.h"
#include "logger.h"
#include "sha256.h"
#include "utils.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void set_error(char *error, size_t error_size, const char *fmt, ...) {
    va_list args;
    if (error == NULL || error_size == 0U) {
        return;
    }
    va_start(args, fmt);
    vsnprintf(error, error_size, fmt, args);
    va_end(args);
}

int append_formatted(char *buffer, size_t buffer_size, size_t *used,
                     const char *fmt, ...) {
    int written;
    va_list args;

    if (buffer == NULL || used == NULL || *used >= buffer_size) {
        return 0;
    }

    va_start(args, fmt);
    written = vsnprintf(buffer + *used, buffer_size - *used, fmt, args);
    va_end(args);

    if (written < 0 || (size_t) written >= buffer_size - *used) {
        return 0;
    }

    *used += (size_t) written;
    return 1;
}

const char *display_type(const char *type) {
    if (type == NULL) return "Unknown";

    if (strcmp(type, "DEPOSIT") == 0) return "Deposit";
    if (strcmp(type, "WITHDRAW") == 0) return "Withdraw";
    if (strcmp(type, "TRANSFER_SENT") == 0) return "Transfer Sent";
    if (strcmp(type, "TRANSFER_RECEIVED") == 0) return "Transfer Received";
    if (strcmp(type, "ADMIN_CREDIT") == 0) return "Admin Credit";
    if (strcmp(type, "ADMIN_DEBIT") == 0) return "Admin Debit";
    if (strcmp(type, "LEGACY") == 0) return "Legacy";

    return type;
}

int db_exec_sql(Database *db, const char *sql,
                char *error, size_t error_size) {
    char *sqlite_error = NULL;

    if (sqlite3_exec(db->conn, sql, NULL, NULL, &sqlite_error) != SQLITE_OK) {
        set_error(error, error_size, "%s",
                  sqlite_error != NULL ? sqlite_error : "SQLite execution failed.");
        sqlite3_free(sqlite_error);
        return 0;
    }

    return 1;
}

int db_count_rows(Database *db, const char *table_name, int *count,
                  char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    char sql[128];
    int rc;

    if (count == NULL) {
        set_error(error, error_size, "Invalid row count request.");
        return 0;
    }

    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s;", table_name);
    rc = sqlite3_prepare_v2(db->conn, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        sqlite3_finalize(stmt);
        return 0;
    }

    *count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return 1;
}

int db_begin_transaction(Database *db, char *error, size_t error_size) {
    return db_exec_sql(db, "BEGIN IMMEDIATE TRANSACTION;", error, error_size);
}

int db_commit_transaction(Database *db, char *error, size_t error_size) {
    return db_exec_sql(db, "COMMIT;", error, error_size);
}

void db_rollback_transaction(Database *db) {
    sqlite3_exec(db->conn, "ROLLBACK;", NULL, NULL, NULL);
}

int db_read_account(Database *db, int account_no, AccountInfo *info,
                    unsigned long long *pin_hash, unsigned long long *salt,
                    int *found, char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;

    if (found == NULL) {
        set_error(error, error_size, "Invalid account lookup.");
        return 0;
    }

    *found = 0;
    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT name, pin_hash, salt, balance, failed_attempts, is_locked "
        "FROM accounts WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        if (info != NULL) {
            const unsigned char *name = sqlite3_column_text(stmt, 0);
            info->account_no = account_no;
            snprintf(info->name, sizeof(info->name), "%s",
                     name != NULL ? (const char *) name : "");
            info->balance = sqlite3_column_double(stmt, 3);
            info->failed_attempts = sqlite3_column_int(stmt, 4);
            info->is_locked = sqlite3_column_int(stmt, 5);
        }

        if (pin_hash != NULL) {
            *pin_hash = (unsigned long long) sqlite3_column_int64(stmt, 1);
        }

        if (salt != NULL) {
            *salt = (unsigned long long) sqlite3_column_int64(stmt, 2);
        }

        *found = 1;
    } else if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return 1;
}

int db_update_login_state(Database *db, int account_no,
                          int failed_attempts, int is_locked,
                          char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        db->conn,
        "UPDATE accounts SET failed_attempts = ?, is_locked = ? WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, failed_attempts);
    sqlite3_bind_int(stmt, 2, is_locked);
    sqlite3_bind_int(stmt, 3, account_no);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

int db_update_balance(Database *db, int account_no, double balance,
                      char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        db->conn,
        "UPDATE accounts SET balance = ? WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_double(stmt, 1, balance);
    sqlite3_bind_int(stmt, 2, account_no);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

int db_insert_transaction(Database *db, int account_no,
                          int has_related, int related_account,
                          const char *type, double amount,
                          int has_balance_after, double balance_after,
                          const char *timestamp,
                          char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;

    rc = sqlite3_prepare_v2(
        db->conn,
        "INSERT INTO transactions "
        "(account_no, related_account, type, amount, balance_after, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?);",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);

    if (has_related) {
        sqlite3_bind_int(stmt, 2, related_account);
    } else {
        sqlite3_bind_null(stmt, 2);
    }

    sqlite3_bind_text(stmt, 3, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, amount);

    if (has_balance_after) {
        sqlite3_bind_double(stmt, 5, balance_after);
    } else {
        sqlite3_bind_null(stmt, 5);
    }

    sqlite3_bind_text(stmt, 6, timestamp, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

static int migrate_legacy_accounts(Database *db, char *error, size_t error_size) {
    FILE *file;
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    int rc;

    typedef struct {
        int accNo;
        char name[50];
        unsigned long pinHash;
        float balance;
    } LegacyAccount;

    if (!db_count_rows(db, "accounts", &count, error, error_size) || count > 0) {
        return count > 0 ? 1 : 0;
    }

    file = fopen("accounts.dat", "rb");
    if (file == NULL) {
        return 1;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "INSERT OR IGNORE INTO accounts "
        "(account_no, name, pin_hash, salt, balance, failed_attempts, is_locked, created_at) "
        "VALUES (?, ?, ?, ?, 0, 0, 0, ?);",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        fclose(file);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    while (1) {
        LegacyAccount account;
        size_t read_count = fread(&account, sizeof(account), 1, file);
        if (read_count != 1) break;

        char timestamp[32];
        unsigned long long salt = generate_salt();
        current_timestamp(timestamp, sizeof(timestamp));

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        sqlite3_bind_int(stmt, 1, account.accNo);
        sqlite3_bind_text(stmt, 2, account.name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64) account.pinHash);
        sqlite3_bind_int64(stmt, 4, (sqlite3_int64) salt);
        sqlite3_bind_text(stmt, 5, timestamp, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            fclose(file);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }
    }

    sqlite3_finalize(stmt);
    fclose(file);
    log_info("Migrated legacy accounts from accounts.dat");
    return 1;
}

static void normalize_legacy_type(const char *input, char *output,
                                  size_t output_size) {
    if (input == NULL || output == NULL || output_size == 0U) return;

    if (strcmp(input, "Deposit") == 0) {
        snprintf(output, output_size, "DEPOSIT");
    } else if (strcmp(input, "Withdraw") == 0) {
        snprintf(output, output_size, "WITHDRAW");
    } else if (strcmp(input, "Transfer Sent") == 0) {
        snprintf(output, output_size, "TRANSFER_SENT");
    } else if (strcmp(input, "Transfer Received") == 0) {
        snprintf(output, output_size, "TRANSFER_RECEIVED");
    } else {
        snprintf(output, output_size, "LEGACY");
    }
}

static int migrate_legacy_transactions(Database *db, char *error, size_t error_size) {
    FILE *file;
    sqlite3_stmt *stmt = NULL;
    char line[256];
    int count = 0;
    int rc;

    if (!db_count_rows(db, "transactions", &count, error, error_size) || count > 0) {
        return count > 0 ? 1 : 0;
    }

    file = fopen("transactions.txt", "r");
    if (file == NULL) {
        return 1;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "INSERT INTO transactions "
        "(account_no, related_account, type, amount, balance_after, created_at) "
        "VALUES (?, NULL, ?, ?, NULL, ?);",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        fclose(file);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char timestamp[32];
        char raw_type[64];
        char normalized_type[32];
        int account_no;
        double amount;

        if (sscanf(line, " %31[^|] | Acc:%d | %63[^|] | Amount: %lf",
                   timestamp, &account_no, raw_type, &amount) != 4) {
            continue;
        }

        normalize_legacy_type(trim_whitespace(raw_type),
                              normalized_type, sizeof(normalized_type));

        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
        sqlite3_bind_int(stmt, 1, account_no);
        sqlite3_bind_text(stmt, 2, normalized_type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(stmt, 3, amount);
        sqlite3_bind_text(stmt, 4, timestamp, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            fclose(file);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }
    }

    sqlite3_finalize(stmt);
    fclose(file);
    log_info("Migrated legacy transactions from transactions.txt");
    return 1;
}

int db_open(Database *db, const char *db_path, char *error, size_t error_size) {
    static const char *schema_sql =
        "PRAGMA foreign_keys = ON;"
        "CREATE TABLE IF NOT EXISTS accounts ("
        "account_no INTEGER PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "pin_hash INTEGER NOT NULL,"
        "salt INTEGER NOT NULL DEFAULT 0,"
        "balance REAL NOT NULL DEFAULT 0,"
        "failed_attempts INTEGER NOT NULL DEFAULT 0,"
        "is_locked INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS admins ("
        "username TEXT PRIMARY KEY,"
        "password_hash INTEGER NOT NULL,"
        "password_salt INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS transactions ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "account_no INTEGER NOT NULL,"
        "related_account INTEGER,"
        "type TEXT NOT NULL,"
        "amount REAL NOT NULL,"
        "balance_after REAL,"
        "created_at TEXT NOT NULL,"
        "FOREIGN KEY(account_no) REFERENCES accounts(account_no)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_transactions_account_time "
        "ON transactions(account_no, created_at DESC, id DESC);";

    int rc;

    if (db == NULL) {
        set_error(error, error_size, "Invalid database handle.");
        return 0;
    }

    db->conn = NULL;
    rc = sqlite3_open(db_path, &db->conn);
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s",
                  db->conn != NULL ? sqlite3_errmsg(db->conn)
                                   : "Unable to open database.");
        db_close(db);
        return 0;
    }

    sqlite3_busy_timeout(db->conn, 5000);

    if (!db_exec_sql(db, schema_sql, error, error_size)) {
        db_close(db);
        return 0;
    }

    migrate_legacy_accounts(db, error, error_size);
    migrate_legacy_transactions(db, error, error_size);

    return 1;
}

void db_close(Database *db) {
    if (db != NULL && db->conn != NULL) {
        sqlite3_close(db->conn);
        db->conn = NULL;
    }
}
