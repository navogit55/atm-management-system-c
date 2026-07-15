#include "account.h"
#include "database.h"
#include "sha256.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

static int resolve_month(const char *input, char *month, size_t month_size) {
    size_t index;

    if (month == NULL || month_size < 8U) {
        return 0;
    }

    if (input == NULL || *input == '\0') {
        current_year_month(month, month_size);
        return 1;
    }

    if (strlen(input) != 7U || input[4] != '-') {
        return 0;
    }

    for (index = 0; index < 7U; index++) {
        if (index == 4U) continue;
        if (input[index] < '0' || input[index] > '9') {
            return 0;
        }
    }

    snprintf(month, month_size, "%s", input);
    return 1;
}

static int apply_balance_transaction(
    Database *db, int account_no, double amount,
    int is_credit, const char *transaction_type,
    double *new_balance, char *error, size_t error_size)
{
    AccountInfo info;
    int found = 0;
    char timestamp[32];
    double updated_balance;

    if (amount <= 0.0) {
        set_error(error, error_size, "Amount must be greater than zero.");
        return 0;
    }

    if (!db_begin_transaction(db, error, error_size)) {
        return 0;
    }

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size) || !found) {
        db_rollback_transaction(db);
        if (found == 0) {
            set_error(error, error_size, "Account not found.");
        }
        return 0;
    }

    if (!is_credit && amount > info.balance) {
        db_rollback_transaction(db);
        set_error(error, error_size, "Insufficient balance.");
        return 0;
    }

    updated_balance = is_credit ? info.balance + amount : info.balance - amount;
    current_timestamp(timestamp, sizeof(timestamp));

    if (!db_update_balance(db, account_no, updated_balance,
                           error, error_size) ||
        !db_insert_transaction(db, account_no, 0, 0, transaction_type,
                               amount, 1, updated_balance, timestamp,
                               error, error_size) ||
        !db_commit_transaction(db, error, error_size)) {
        db_rollback_transaction(db);
        return 0;
    }

    if (new_balance != NULL) {
        *new_balance = updated_balance;
    }

    return 1;
}

int db_get_balance(Database *db, int account_no, double *balance,
                   char *error, size_t error_size) {
    AccountInfo info;
    int found = 0;

    if (balance == NULL) {
        set_error(error, error_size, "Invalid balance request.");
        return 0;
    }

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    *balance = info.balance;
    return 1;
}

int db_deposit(Database *db, int account_no, double amount,
               double *new_balance, char *error, size_t error_size) {
    if (amount <= 0.0) {
        set_error(error, error_size,
                  "Deposit amount must be greater than zero.");
        return 0;
    }

    return apply_balance_transaction(db, account_no, amount, 1, "DEPOSIT",
                                     new_balance, error, error_size);
}

int db_withdraw(Database *db, int account_no, double amount,
                double *new_balance, char *error, size_t error_size) {
    if (amount <= 0.0) {
        set_error(error, error_size,
                  "Withdrawal amount must be greater than zero.");
        return 0;
    }

    return apply_balance_transaction(db, account_no, amount, 0, "WITHDRAW",
                                     new_balance, error, error_size);
}

int db_transfer(Database *db, int from_account, int to_account,
                double amount, double *new_balance,
                char *error, size_t error_size) {
    AccountInfo sender_info;
    AccountInfo receiver_info;
    int sender_found = 0;
    int receiver_found = 0;
    char timestamp[32];
    double sender_balance;
    double receiver_balance;

    if (to_account <= 0 || to_account == from_account) {
        set_error(error, error_size,
                  "Receiver account number must be different from your own.");
        return 0;
    }

    if (amount <= 0.0) {
        set_error(error, error_size,
                  "Transfer amount must be greater than zero.");
        return 0;
    }

    if (!db_begin_transaction(db, error, error_size)) {
        return 0;
    }

    if (!db_read_account(db, from_account, &sender_info, NULL, NULL,
                         &sender_found, error, error_size) || !sender_found) {
        db_rollback_transaction(db);
        if (sender_found == 0) {
            set_error(error, error_size, "Sender account not found.");
        }
        return 0;
    }

    if (!db_read_account(db, to_account, &receiver_info, NULL, NULL,
                         &receiver_found, error, error_size) || !receiver_found) {
        db_rollback_transaction(db);
        if (receiver_found == 0) {
            set_error(error, error_size, "Receiver account not found.");
        }
        return 0;
    }

    if (amount > sender_info.balance) {
        db_rollback_transaction(db);
        set_error(error, error_size, "Insufficient balance.");
        return 0;
    }

    sender_balance = sender_info.balance - amount;
    receiver_balance = receiver_info.balance + amount;
    current_timestamp(timestamp, sizeof(timestamp));

    if (!db_update_balance(db, from_account, sender_balance,
                           error, error_size) ||
        !db_update_balance(db, to_account, receiver_balance,
                           error, error_size) ||
        !db_insert_transaction(db, from_account, 1, to_account,
                               "TRANSFER_SENT", amount, 1, sender_balance,
                               timestamp, error, error_size) ||
        !db_insert_transaction(db, to_account, 1, from_account,
                               "TRANSFER_RECEIVED", amount, 1, receiver_balance,
                               timestamp, error, error_size) ||
        !db_commit_transaction(db, error, error_size)) {
        db_rollback_transaction(db);
        return 0;
    }

    if (new_balance != NULL) {
        *new_balance = sender_balance;
    }

    return 1;
}

int db_build_mini_statement(Database *db, int account_no,
                            char *output, size_t output_size,
                            char *error, size_t error_size) {
    AccountInfo info;
    sqlite3_stmt *stmt = NULL;
    int found = 0;
    int rc;
    size_t used = 0U;

    if (output == NULL || output_size == 0U) {
        set_error(error, error_size, "Invalid statement output buffer.");
        return 0;
    }

    output[0] = '\0';

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    if (!append_formatted(output, output_size, &used,
            "MINI STATEMENT\n"
            "Account Number: %d\n"
            "Account Name  : %s\n"
            "Current Balance: %.2f\n\n"
            "%-19s %-18s %-12s %-12s %s\n",
            info.account_no, info.name, info.balance,
            "Timestamp", "Type", "Amount", "Balance", "Details")) {
        set_error(error, error_size,
                  "Mini statement output exceeded buffer size.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT created_at, type, amount, balance_after, related_account "
        "FROM transactions WHERE account_no = ? "
        "ORDER BY created_at DESC, id DESC LIMIT ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    sqlite3_bind_int(stmt, 2, ATM_STATEMENT_LIMIT);

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        append_formatted(output, output_size, &used,
                         "No transactions found.\n");
        sqlite3_finalize(stmt);
        return 1;
    }

    while (rc == SQLITE_ROW) {
        const char *timestamp = (const char *) sqlite3_column_text(stmt, 0);
        const char *type = (const char *) sqlite3_column_text(stmt, 1);
        double amount = sqlite3_column_double(stmt, 2);
        int related_is_null = sqlite3_column_type(stmt, 4) == SQLITE_NULL;
        int balance_is_null = sqlite3_column_type(stmt, 3) == SQLITE_NULL;
        char details[64];
        char balance_text[32];

        if (related_is_null) {
            snprintf(details, sizeof(details), "-");
        } else {
            snprintf(details, sizeof(details), "Other: %d",
                     sqlite3_column_int(stmt, 4));
        }

        if (balance_is_null) {
            snprintf(balance_text, sizeof(balance_text), "n/a");
        } else {
            snprintf(balance_text, sizeof(balance_text), "%.2f",
                     sqlite3_column_double(stmt, 3));
        }

        if (!append_formatted(output, output_size, &used,
                "%-19s %-18s %-12.2f %-12s %s\n",
                timestamp != NULL ? timestamp : "unknown",
                display_type(type), amount,
                balance_text, details)) {
            sqlite3_finalize(stmt);
            set_error(error, error_size,
                      "Mini statement output exceeded buffer size.");
            return 0;
        }

        rc = sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

int db_build_monthly_summary(Database *db, int account_no,
                             const char *month, char *output,
                             size_t output_size,
                             char *error, size_t error_size) {
    AccountInfo info;
    sqlite3_stmt *stmt = NULL;
    int found = 0;
    char resolved_month[8];
    int rc;
    int transaction_count = 0;
    double deposits = 0.0;
    double withdrawals = 0.0;
    double transfers_sent = 0.0;
    double transfers_received = 0.0;
    double net_change;

    if (output == NULL || output_size == 0U) {
        set_error(error, error_size, "Invalid summary output buffer.");
        return 0;
    }

    output[0] = '\0';

    if (!resolve_month(month, resolved_month, sizeof(resolved_month))) {
        set_error(error, error_size, "Month must use YYYY-MM format.");
        return 0;
    }

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT "
        "COUNT(*), "
        "COALESCE(SUM(CASE WHEN type = 'DEPOSIT' THEN amount ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN type = 'WITHDRAW' THEN amount ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN type = 'TRANSFER_SENT' THEN amount ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN type = 'TRANSFER_RECEIVED' THEN amount ELSE 0 END), 0) "
        "FROM transactions "
        "WHERE account_no = ? AND substr(created_at, 1, 7) = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    sqlite3_bind_text(stmt, 2, resolved_month, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    transaction_count = sqlite3_column_int(stmt, 0);
    deposits = sqlite3_column_double(stmt, 1);
    withdrawals = sqlite3_column_double(stmt, 2);
    transfers_sent = sqlite3_column_double(stmt, 3);
    transfers_received = sqlite3_column_double(stmt, 4);
    sqlite3_finalize(stmt);

    net_change = deposits + transfers_received - withdrawals - transfers_sent;

    snprintf(output, output_size,
        "MONTHLY SUMMARY\n"
        "Account Number   : %d\n"
        "Account Name     : %s\n"
        "Period           : %s\n"
        "Transactions     : %d\n"
        "Deposits         : %.2f\n"
        "Withdrawals      : %.2f\n"
        "Transfers Sent   : %.2f\n"
        "Transfers Received: %.2f\n"
        "Net Change       : %.2f\n"
        "Current Balance  : %.2f\n",
        info.account_no, info.name, resolved_month,
        transaction_count, deposits, withdrawals,
        transfers_sent, transfers_received,
        net_change, info.balance);

    return 1;
}

int db_lock_account(Database *db, int account_no,
                    char *error, size_t error_size) {
    AccountInfo info;
    sqlite3_stmt *stmt = NULL;
    int found = 0;
    int rc;

    if (account_no <= 0) {
        set_error(error, error_size, "Account number must be positive.");
        return 0;
    }

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "UPDATE accounts SET failed_attempts = 3, is_locked = 1 "
        "WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

int db_unlock_account(Database *db, int account_no,
                      char *error, size_t error_size) {
    AccountInfo info;
    sqlite3_stmt *stmt = NULL;
    int found = 0;
    int rc;

    if (account_no <= 0) {
        set_error(error, error_size, "Account number must be positive.");
        return 0;
    }

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "UPDATE accounts SET failed_attempts = 0, is_locked = 0 "
        "WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

int db_admin_credit(Database *db, int account_no, double amount,
                    double *new_balance, char *error, size_t error_size) {
    if (amount <= 0.0) {
        set_error(error, error_size,
                  "Credit amount must be greater than zero.");
        return 0;
    }
    return apply_balance_transaction(db, account_no, amount, 1,
                                     "ADMIN_CREDIT", new_balance,
                                     error, error_size);
}

int db_admin_debit(Database *db, int account_no, double amount,
                   double *new_balance, char *error, size_t error_size) {
    if (amount <= 0.0) {
        set_error(error, error_size,
                  "Debit amount must be greater than zero.");
        return 0;
    }
    return apply_balance_transaction(db, account_no, amount, 0,
                                     "ADMIN_DEBIT", new_balance,
                                     error, error_size);
}

int db_edit_account(Database *db, int current_account_no, int new_account_no,
                    const char *new_name, int new_pin,
                    char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    char name_copy[ATM_NAME_LENGTH];
    char created_at[32];
    char *trimmed_name;
    double balance = 0.0;
    int failed_attempts = 0;
    int is_locked = 0;
    int rc;

    if (db == NULL || db->conn == NULL) {
        set_error(error, error_size, "Database is not available.");
        return 0;
    }

    if (current_account_no <= 0 || new_account_no <= 0) {
        set_error(error, error_size, "Account number must be positive.");
        return 0;
    }

    if (new_name == NULL) {
        set_error(error, error_size, "Account name is required.");
        return 0;
    }

    snprintf(name_copy, sizeof(name_copy), "%s", new_name);
    trimmed_name = trim_whitespace(name_copy);
    if (*trimmed_name == '\0') {
        set_error(error, error_size, "Account name is required.");
        return 0;
    }

    if (new_pin < 1000 || new_pin > 9999) {
        set_error(error, error_size, "PIN must be a 4-digit number.");
        return 0;
    }

    if (!db_begin_transaction(db, error, error_size)) {
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT balance, failed_attempts, is_locked, created_at "
        "FROM accounts WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, current_account_no);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        db_rollback_transaction(db);
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    balance = sqlite3_column_double(stmt, 0);
    failed_attempts = sqlite3_column_int(stmt, 1);
    is_locked = sqlite3_column_int(stmt, 2);
    snprintf(created_at, sizeof(created_at), "%s",
             sqlite3_column_text(stmt, 3) != NULL
                 ? (const char *) sqlite3_column_text(stmt, 3)
                 : "1970-01-01 00:00:00");
    sqlite3_finalize(stmt);

    unsigned long long new_salt = generate_salt();

    char pin_str[16];
    snprintf(pin_str, sizeof(pin_str), "%d", new_pin);

    uint8_t hash_input[512];
    size_t total = 0;
    size_t pin_len = strlen(pin_str);
    if (total + sizeof(new_salt) <= sizeof(hash_input)) {
        memcpy(hash_input + total, &new_salt, sizeof(new_salt));
        total += sizeof(new_salt);
    }
    if (total + pin_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, pin_str, pin_len);
        total += pin_len;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256(hash_input, total, hash);

    unsigned long long pin_hash = 0;
    memcpy(&pin_hash, hash, sizeof(pin_hash));

    if (current_account_no == new_account_no) {
        rc = sqlite3_prepare_v2(
            db->conn,
            "UPDATE accounts SET name = ?, pin_hash = ?, salt = ? "
            "WHERE account_no = ?;",
            -1, &stmt, NULL
        );
        if (rc != SQLITE_OK) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        sqlite3_bind_text(stmt, 1, trimmed_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, (sqlite3_int64) pin_hash);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64) new_salt);
        sqlite3_bind_int(stmt, 4, current_account_no);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }
    } else {
        rc = sqlite3_prepare_v2(
            db->conn,
            "INSERT INTO accounts "
            "(account_no, name, pin_hash, salt, balance, "
            "failed_attempts, is_locked, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?);",
            -1, &stmt, NULL
        );
        if (rc != SQLITE_OK) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        sqlite3_bind_int(stmt, 1, new_account_no);
        sqlite3_bind_text(stmt, 2, trimmed_name, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 3, (sqlite3_int64) pin_hash);
        sqlite3_bind_int64(stmt, 4, (sqlite3_int64) new_salt);
        sqlite3_bind_double(stmt, 5, balance);
        sqlite3_bind_int(stmt, 6, failed_attempts);
        sqlite3_bind_int(stmt, 7, is_locked);
        sqlite3_bind_text(stmt, 8, created_at, -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            db_rollback_transaction(db);
            if (rc == SQLITE_CONSTRAINT) {
                set_error(error, error_size,
                          "New account number already exists.");
            } else {
                set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            }
            return 0;
        }

        rc = sqlite3_prepare_v2(
            db->conn,
            "UPDATE transactions SET account_no = ? WHERE account_no = ?;",
            -1, &stmt, NULL
        );
        if (rc != SQLITE_OK) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        sqlite3_bind_int(stmt, 1, new_account_no);
        sqlite3_bind_int(stmt, 2, current_account_no);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        rc = sqlite3_prepare_v2(
            db->conn,
            "UPDATE transactions SET related_account = ? "
            "WHERE related_account = ?;",
            -1, &stmt, NULL
        );
        if (rc != SQLITE_OK) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        sqlite3_bind_int(stmt, 1, new_account_no);
        sqlite3_bind_int(stmt, 2, current_account_no);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        rc = sqlite3_prepare_v2(
            db->conn,
            "DELETE FROM accounts WHERE account_no = ?;",
            -1, &stmt, NULL
        );
        if (rc != SQLITE_OK) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }

        sqlite3_bind_int(stmt, 1, current_account_no);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc != SQLITE_DONE) {
            db_rollback_transaction(db);
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
            return 0;
        }
    }

    if (!db_commit_transaction(db, error, error_size)) {
        db_rollback_transaction(db);
        return 0;
    }

    return 1;
}

int db_delete_account(Database *db, int account_no,
                      char *error, size_t error_size) {
    AccountInfo info;
    sqlite3_stmt *stmt = NULL;
    int found = 0;
    int rc;

    if (account_no <= 0) {
        set_error(error, error_size, "Account number must be positive.");
        return 0;
    }

    if (!db_read_account(db, account_no, &info, NULL, NULL,
                         &found, error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    if (info.balance != 0.0) {
        set_error(error, error_size,
                  "Account balance must be zero before deletion.");
        return 0;
    }

    if (!db_begin_transaction(db, error, error_size)) {
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "UPDATE transactions SET related_account = NULL "
        "WHERE related_account = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "DELETE FROM transactions WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "DELETE FROM accounts WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        db_rollback_transaction(db);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    if (!db_commit_transaction(db, error, error_size)) {
        db_rollback_transaction(db);
        return 0;
    }

    return 1;
}

int db_build_account_listing(Database *db, char *output, size_t output_size,
                             char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;
    size_t used = 0U;

    if (output == NULL || output_size == 0U) {
        set_error(error, error_size, "Invalid account listing buffer.");
        return 0;
    }

    output[0] = '\0';
    if (!append_formatted(output, output_size, &used,
            "ALL ACCOUNTS\n\n%-10s %-24s %-12s %-10s %s\n",
            "Acc No", "Name", "Balance", "Locked", "Failed Attempts")) {
        set_error(error, error_size,
                  "Account listing output exceeded buffer size.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT account_no, name, balance, is_locked, failed_attempts "
        "FROM accounts ORDER BY account_no ASC;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        append_formatted(output, output_size, &used, "No accounts found.\n");
        sqlite3_finalize(stmt);
        return 1;
    }

    while (rc == SQLITE_ROW) {
        int account_no = sqlite3_column_int(stmt, 0);
        const char *name = (const char *) sqlite3_column_text(stmt, 1);
        double balance = sqlite3_column_double(stmt, 2);
        int is_locked = sqlite3_column_int(stmt, 3);
        int failed_attempts = sqlite3_column_int(stmt, 4);

        if (!append_formatted(output, output_size, &used,
                "%-10d %-24.24s %-12.2f %-10s %d\n",
                account_no, name != NULL ? name : "", balance,
                is_locked ? "Yes" : "No", failed_attempts)) {
            sqlite3_finalize(stmt);
            set_error(error, error_size,
                      "Account listing output exceeded buffer size.");
            return 0;
        }

        rc = sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}

int db_build_account_details(Database *db, int account_no,
                             char *output, size_t output_size,
                             char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;
    char created_at[32];
    char name[ATM_NAME_LENGTH];
    double balance;
    int failed_attempts;
    int is_locked;
    char latest_activity[128];

    if (output == NULL || output_size == 0U) {
        set_error(error, error_size, "Invalid account details buffer.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT name, balance, failed_attempts, is_locked, created_at "
        "FROM accounts WHERE account_no = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        set_error(error, error_size, "Account not found.");
        return 0;
    }

    snprintf(name, sizeof(name), "%s",
             sqlite3_column_text(stmt, 0) != NULL
                 ? (const char *) sqlite3_column_text(stmt, 0) : "");
    balance = sqlite3_column_double(stmt, 1);
    failed_attempts = sqlite3_column_int(stmt, 2);
    is_locked = sqlite3_column_int(stmt, 3);
    snprintf(created_at, sizeof(created_at), "%s",
             sqlite3_column_text(stmt, 4) != NULL
                 ? (const char *) sqlite3_column_text(stmt, 4) : "unknown");
    sqlite3_finalize(stmt);

    snprintf(latest_activity, sizeof(latest_activity), "No transactions yet");
    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT created_at, type, amount FROM transactions "
        "WHERE account_no = ? ORDER BY created_at DESC, id DESC LIMIT 1;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        snprintf(latest_activity, sizeof(latest_activity),
                 "%s | %s | %.2f",
                 sqlite3_column_text(stmt, 0) != NULL
                     ? (const char *) sqlite3_column_text(stmt, 0) : "unknown",
                 display_type((const char *) sqlite3_column_text(stmt, 1)),
                 sqlite3_column_double(stmt, 2));
    }

    sqlite3_finalize(stmt);

    snprintf(output, output_size,
        "ACCOUNT DETAILS\n"
        "Account Number : %d\n"
        "Name           : %s\n"
        "Balance        : %.2f\n"
        "Locked         : %s\n"
        "Failed Attempts: %d\n"
        "Created At     : %s\n"
        "Latest Activity: %s\n",
        account_no, name, balance,
        is_locked ? "Yes" : "No",
        failed_attempts, created_at, latest_activity);

    return 1;
}

int db_build_system_summary(Database *db, char *output, size_t output_size,
                            char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;
    char today[11];
    char timestamp[32];
    int total_accounts;
    int locked_accounts;
    double total_balance;
    int total_transactions;
    int todays_transactions;
    int total_admins;
    int top_account_no = 0;
    double top_balance = 0.0;
    char top_name[ATM_NAME_LENGTH];

    if (output == NULL || output_size == 0U) {
        set_error(error, error_size, "Invalid system summary buffer.");
        return 0;
    }

    current_timestamp(timestamp, sizeof(timestamp));
    memcpy(today, timestamp, 10U);
    today[10] = '\0';
    snprintf(top_name, sizeof(top_name), "N/A");

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT "
        "(SELECT COUNT(*) FROM accounts), "
        "(SELECT COUNT(*) FROM accounts WHERE is_locked = 1), "
        "(SELECT COALESCE(SUM(balance), 0) FROM accounts), "
        "(SELECT COUNT(*) FROM transactions), "
        "(SELECT COUNT(*) FROM transactions WHERE substr(created_at, 1, 10) = ?), "
        "(SELECT COUNT(*) FROM admins);",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, today, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    total_accounts = sqlite3_column_int(stmt, 0);
    locked_accounts = sqlite3_column_int(stmt, 1);
    total_balance = sqlite3_column_double(stmt, 2);
    total_transactions = sqlite3_column_int(stmt, 3);
    todays_transactions = sqlite3_column_int(stmt, 4);
    total_admins = sqlite3_column_int(stmt, 5);
    sqlite3_finalize(stmt);

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT account_no, name, balance FROM accounts "
        "ORDER BY balance DESC, account_no ASC LIMIT 1;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        top_account_no = sqlite3_column_int(stmt, 0);
        snprintf(top_name, sizeof(top_name), "%s",
                 sqlite3_column_text(stmt, 1) != NULL
                     ? (const char *) sqlite3_column_text(stmt, 1) : "");
        top_balance = sqlite3_column_double(stmt, 2);
    }

    sqlite3_finalize(stmt);

    snprintf(output, output_size,
        "ADMIN DASHBOARD\n"
        "Total Accounts     : %d\n"
        "Locked Accounts    : %d\n"
        "Total Bank Balance : %.2f\n"
        "Total Transactions : %d\n"
        "Today's Transactions: %d\n"
        "Admin Users        : %d\n"
        "Highest Balance    : %d | %s | %.2f\n",
        total_accounts, locked_accounts, total_balance,
        total_transactions, todays_transactions, total_admins,
        top_account_no, top_name, top_balance);

    return 1;
}

int db_build_recent_transactions(Database *db, int limit,
                                 char *output, size_t output_size,
                                 char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int rc;
    size_t used = 0U;

    if (output == NULL || output_size == 0U) {
        set_error(error, error_size, "Invalid recent transaction buffer.");
        return 0;
    }

    if (limit <= 0) limit = 10;
    else if (limit > 25) limit = 25;

    output[0] = '\0';
    if (!append_formatted(output, output_size, &used,
            "RECENT TRANSACTIONS\n\n%-19s %-10s %-18s %-12s %s\n",
            "Timestamp", "Acc No", "Type", "Amount", "Details")) {
        set_error(error, error_size,
                  "Recent transaction output exceeded buffer size.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT created_at, account_no, related_account, type, amount "
        "FROM transactions ORDER BY created_at DESC, id DESC LIMIT ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, limit);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        append_formatted(output, output_size, &used,
                         "No transactions found.\n");
        sqlite3_finalize(stmt);
        return 1;
    }

    while (rc == SQLITE_ROW) {
        const char *timestamp_text = (const char *) sqlite3_column_text(stmt, 0);
        int tx_account_no = sqlite3_column_int(stmt, 1);
        int has_related = sqlite3_column_type(stmt, 2) != SQLITE_NULL;
        const char *type = (const char *) sqlite3_column_text(stmt, 3);
        double amount = sqlite3_column_double(stmt, 4);
        char details[64];

        if (has_related) {
            snprintf(details, sizeof(details), "Other: %d",
                     sqlite3_column_int(stmt, 2));
        } else {
            snprintf(details, sizeof(details), "-");
        }

        if (!append_formatted(output, output_size, &used,
                "%-19s %-10d %-18s %-12.2f %s\n",
                timestamp_text != NULL ? timestamp_text : "unknown",
                tx_account_no, display_type(type), amount, details)) {
            sqlite3_finalize(stmt);
            set_error(error, error_size,
                      "Recent transaction output exceeded buffer size.");
            return 0;
        }

        rc = sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    return 1;
}
