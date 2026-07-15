#include "auth.h"
#include "database.h"
#include "logger.h"
#include "sha256.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int seed_default_admin(Database *db, char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    int count = 0;
    int rc;

    if (!db_count_rows(db, "admins", &count, error, error_size)) {
        return 0;
    }

    if (count > 0) {
        return 1;
    }

    const char *admin_user = getenv("ATM_ADMIN_USERNAME");
    const char *admin_pass = getenv("ATM_ADMIN_PASSWORD");

    if (admin_user == NULL || *admin_user == '\0' ||
        admin_pass == NULL || *admin_pass == '\0') {
        set_error(error, error_size,
                  "ATM_ADMIN_USERNAME and ATM_ADMIN_PASSWORD environment "
                  "variables must be set on first run.");
        return 0;
    }

    if (strlen(admin_pass) < 8) {
        set_error(error, error_size,
                  "Admin password must be at least 8 characters.");
        return 0;
    }

    unsigned long long salt = generate_salt();

    uint8_t hash_input[512];
    size_t user_len = strlen(admin_user);
    size_t pass_len = strlen(admin_pass);

    size_t total = 0;
    if (total + sizeof(salt) <= sizeof(hash_input)) {
        memcpy(hash_input + total, &salt, sizeof(salt));
        total += sizeof(salt);
    }
    if (total + user_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, admin_user, user_len);
        total += user_len;
    }
    if (total + pass_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, admin_pass, pass_len);
        total += pass_len;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256(hash_input, total, hash);

    unsigned long long password_hash = 0;
    memcpy(&password_hash, hash, sizeof(password_hash));

    rc = sqlite3_prepare_v2(
        db->conn,
        "INSERT INTO admins (username, password_hash, password_salt, created_at) "
        "VALUES (?, ?, ?, ?);",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, admin_user, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64) password_hash);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64) salt);

    {
        char timestamp[32];
        current_timestamp(timestamp, sizeof(timestamp));
        sqlite3_bind_text(stmt, 4, timestamp, -1, SQLITE_TRANSIENT);
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    log_info("Default admin account created for user '%s'", admin_user);
    return 1;
}

int db_create_account(Database *db, int account_no, const char *name,
                      int pin, char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    char name_copy[ATM_NAME_LENGTH];
    char timestamp[32];
    char *trimmed_name;
    int rc;

    if (db == NULL || db->conn == NULL) {
        set_error(error, error_size, "Database is not available.");
        return 0;
    }

    if (account_no <= 0) {
        set_error(error, error_size, "Account number must be positive.");
        return 0;
    }

    if (pin < 1000 || pin > 9999) {
        set_error(error, error_size, "PIN must be a 4-digit number.");
        return 0;
    }

    if (name == NULL) {
        set_error(error, error_size, "Account name is required.");
        return 0;
    }

    snprintf(name_copy, sizeof(name_copy), "%s", name);
    trimmed_name = trim_whitespace(name_copy);
    if (*trimmed_name == '\0') {
        set_error(error, error_size, "Account name is required.");
        return 0;
    }

    unsigned long long salt = generate_salt();

    char pin_str[16];
    snprintf(pin_str, sizeof(pin_str), "%d", pin);

    uint8_t hash_input[512];
    size_t total = 0;
    size_t pin_len = strlen(pin_str);

    if (total + sizeof(salt) <= sizeof(hash_input)) {
        memcpy(hash_input + total, &salt, sizeof(salt));
        total += sizeof(salt);
    }
    if (total + pin_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, pin_str, pin_len);
        total += pin_len;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256(hash_input, total, hash);

    unsigned long long pin_hash = 0;
    memcpy(&pin_hash, hash, sizeof(pin_hash));

    current_timestamp(timestamp, sizeof(timestamp));

    rc = sqlite3_prepare_v2(
        db->conn,
        "INSERT INTO accounts "
        "(account_no, name, pin_hash, salt, balance, failed_attempts, is_locked, created_at) "
        "VALUES (?, ?, ?, ?, 0, 0, 0, ?);",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int(stmt, 1, account_no);
    sqlite3_bind_text(stmt, 2, trimmed_name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64) pin_hash);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64) salt);
    sqlite3_bind_text(stmt, 5, timestamp, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        if (rc == SQLITE_CONSTRAINT) {
            set_error(error, error_size, "Account number already exists.");
        } else {
            set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        }
        return 0;
    }

    return 1;
}

int db_login(Database *db, int account_no, int pin, AccountInfo *info,
             char *error, size_t error_size) {
    AccountInfo account_info;
    unsigned long long stored_pin_hash = 0ULL;
    unsigned long long salt = 0ULL;
    int found = 0;

    if (pin < 1000 || pin > 9999) {
        set_error(error, error_size, "PIN must be a 4-digit number.");
        return 0;
    }

    if (!db_read_account(db, account_no, &account_info,
                         &stored_pin_hash, &salt, &found,
                         error, error_size)) {
        return 0;
    }

    if (!found) {
        set_error(error, error_size, "Invalid account number or PIN.");
        return 0;
    }

    if (account_info.is_locked) {
        set_error(error, error_size,
                  "Account is locked after 3 failed login attempts. "
                  "Contact an admin.");
        return 0;
    }

    char pin_str[16];
    snprintf(pin_str, sizeof(pin_str), "%d", pin);

    uint8_t hash_input[512];
    size_t total = 0;
    size_t pin_len = strlen(pin_str);

    if (total + sizeof(salt) <= sizeof(hash_input)) {
        memcpy(hash_input + total, &salt, sizeof(salt));
        total += sizeof(salt);
    }
    if (total + pin_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, pin_str, pin_len);
        total += pin_len;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256(hash_input, total, hash);

    unsigned long long computed_hash = 0;
    memcpy(&computed_hash, hash, sizeof(computed_hash));

    if (stored_pin_hash != computed_hash) {
        int failed_attempts = account_info.failed_attempts + 1;
        int is_locked = failed_attempts >= 3;

        if (!db_update_login_state(db, account_no, failed_attempts,
                                   is_locked, error, error_size)) {
            return 0;
        }

        if (is_locked) {
            set_error(error, error_size,
                      "Account locked after 3 failed login attempts.");
        } else {
            set_error(error, error_size,
                      "Invalid account number or PIN. Remaining attempts: %d.",
                      3 - failed_attempts);
        }
        return 0;
    }

    if (!db_update_login_state(db, account_no, 0, 0, error, error_size)) {
        return 0;
    }

    if (info != NULL) {
        *info = account_info;
        info->failed_attempts = 0;
        info->is_locked = 0;
    }

    return 1;
}

int db_admin_login(Database *db, const char *username, const char *password,
                   char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    unsigned long long stored_hash = 0ULL;
    unsigned long long stored_salt = 0ULL;
    int rc;

    if (username == NULL || password == NULL ||
        *username == '\0' || *password == '\0') {
        set_error(error, error_size, "Admin username and password are required.");
        return 0;
    }

    rc = sqlite3_prepare_v2(
        db->conn,
        "SELECT password_hash, password_salt FROM admins WHERE username = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_text(stmt, 1, username, -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        set_error(error, error_size, "Invalid admin credentials.");
        return 0;
    }

    stored_hash = (unsigned long long) sqlite3_column_int64(stmt, 0);
    stored_salt = (unsigned long long) sqlite3_column_int64(stmt, 1);
    sqlite3_finalize(stmt);

    size_t user_len = strlen(username);
    size_t pass_len = strlen(password);

    uint8_t hash_input[512];
    size_t total = 0;
    if (total + sizeof(stored_salt) <= sizeof(hash_input)) {
        memcpy(hash_input + total, &stored_salt, sizeof(stored_salt));
        total += sizeof(stored_salt);
    }
    if (total + user_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, username, user_len);
        total += user_len;
    }
    if (total + pass_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, password, pass_len);
        total += pass_len;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256(hash_input, total, hash);

    unsigned long long computed_hash = 0;
    memcpy(&computed_hash, hash, sizeof(computed_hash));

    if (stored_hash != computed_hash) {
        set_error(error, error_size, "Invalid admin credentials.");
        return 0;
    }

    return 1;
}

int db_change_admin_password(Database *db, const char *username,
                             const char *new_password,
                             char *error, size_t error_size) {
    sqlite3_stmt *stmt = NULL;
    char password_copy[128];
    char *trimmed_password;
    int rc;

    if (username == NULL || *username == '\0') {
        set_error(error, error_size, "Admin username is required.");
        return 0;
    }

    if (new_password == NULL) {
        set_error(error, error_size, "New admin password is required.");
        return 0;
    }

    snprintf(password_copy, sizeof(password_copy), "%s", new_password);
    trimmed_password = trim_whitespace(password_copy);
    if (*trimmed_password == '\0') {
        set_error(error, error_size, "New admin password is required.");
        return 0;
    }

    if (strlen(trimmed_password) < 8) {
        set_error(error, error_size,
                  "New admin password must be at least 8 characters.");
        return 0;
    }

    unsigned long long new_salt = generate_salt();

    size_t user_len = strlen(username);
    size_t pass_len = strlen(trimmed_password);

    uint8_t hash_input[512];
    size_t total = 0;
    if (total + sizeof(new_salt) <= sizeof(hash_input)) {
        memcpy(hash_input + total, &new_salt, sizeof(new_salt));
        total += sizeof(new_salt);
    }
    if (total + user_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, username, user_len);
        total += user_len;
    }
    if (total + pass_len <= sizeof(hash_input)) {
        memcpy(hash_input + total, trimmed_password, pass_len);
        total += pass_len;
    }

    uint8_t hash[SHA256_DIGEST_SIZE];
    sha256(hash_input, total, hash);

    unsigned long long password_hash = 0;
    memcpy(&password_hash, hash, sizeof(password_hash));

    rc = sqlite3_prepare_v2(
        db->conn,
        "UPDATE admins SET password_hash = ?, password_salt = ? WHERE username = ?;",
        -1, &stmt, NULL
    );
    if (rc != SQLITE_OK) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, (sqlite3_int64) password_hash);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64) new_salt);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        set_error(error, error_size, "%s", sqlite3_errmsg(db->conn));
        return 0;
    }

    if (sqlite3_changes(db->conn) == 0) {
        set_error(error, error_size, "Admin account not found.");
        return 0;
    }

    return 1;
}

int db_seed_admin(Database *db, char *error, size_t error_size) {
    return seed_default_admin(db, error, error_size);
}
