#include "account.h"
#include "auth.h"
#include "database.h"
#include "logger.h"
#include "protocol.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int customer_account_no;
    int is_admin;
    char customer_name[ATM_NAME_LENGTH];
    char admin_username[64];
} Session;

static void reset_session(Session *session) {
    if (session == NULL) return;
    session->customer_account_no = 0;
    session->is_admin = 0;
    session->customer_name[0] = '\0';
    session->admin_username[0] = '\0';
}

static int split_lines(char *text, char **lines, int max_lines) {
    int count = 0;
    char *cursor = text;

    while (cursor != NULL && count < max_lines) {
        char *newline = strchr(cursor, '\n');
        lines[count++] = cursor;
        if (newline == NULL) break;
        *newline = '\0';
        cursor = newline + 1;
    }

    return count;
}

static void make_response(char *response, size_t response_size,
                          const char *status, const char *payload) {
    snprintf(response, response_size, "%s\n%s",
             status, payload != NULL ? payload : "");
}

static int require_customer_session(const Session *session,
                                    char *response, size_t response_size) {
    if (session != NULL && session->customer_account_no > 0 && !session->is_admin) {
        return 1;
    }
    make_response(response, response_size, "ERR", "Customer login required.");
    return 0;
}

static int require_admin_session(const Session *session,
                                 char *response, size_t response_size) {
    if (session != NULL && session->is_admin) {
        return 1;
    }
    make_response(response, response_size, "ERR", "Admin login required.");
    return 0;
}

static void handle_request(Database *db, Session *session,
                           char *request,
                           char *response, size_t response_size) {
    char *lines[10];
    int line_count = split_lines(request, lines, 10);
    int account_no;
    int pin;
    double amount;
    char error[256];

    if (line_count <= 0 || lines[0][0] == '\0') {
        make_response(response, response_size, "ERR", "Empty request.");
        return;
    }

    if (strcmp(lines[0], "CREATE_ACCOUNT") == 0) {
        if (line_count < 4 || !parse_int(lines[1], &account_no)
            || !parse_int(lines[3], &pin)) {
            make_response(response, response_size, "ERR",
                          "CREATE_ACCOUNT requires account number, name, and 4-digit PIN.");
            return;
        }

        if (!db_create_account(db, account_no, lines[2], pin,
                               error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK",
                      "Account created successfully.");
        return;
    }

    if (strcmp(lines[0], "LOGIN") == 0) {
        AccountInfo info;

        if (line_count < 3 || !parse_int(lines[1], &account_no)
            || !parse_int(lines[2], &pin)) {
            make_response(response, response_size, "ERR",
                          "LOGIN requires account number and PIN.");
            return;
        }

        if (!db_login(db, account_no, pin, &info,
                      error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        reset_session(session);
        session->customer_account_no = info.account_no;
        snprintf(session->customer_name, sizeof(session->customer_name),
                 "%s", info.name);
        make_response(response, response_size, "OK", session->customer_name);
        return;
    }

    if (strcmp(lines[0], "ADMIN_LOGIN") == 0) {
        if (line_count < 3) {
            make_response(response, response_size, "ERR",
                          "ADMIN_LOGIN requires username and password.");
            return;
        }

        if (!db_admin_login(db, lines[1], lines[2],
                            error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        reset_session(session);
        session->is_admin = 1;
        snprintf(session->admin_username, sizeof(session->admin_username),
                 "%s", lines[1]);
        make_response(response, response_size, "OK",
                      "Admin login successful.");
        return;
    }

    if (strcmp(lines[0], "LOGOUT") == 0) {
        reset_session(session);
        make_response(response, response_size, "OK", "Logged out.");
        return;
    }

    if (strcmp(lines[0], "BALANCE") == 0) {
        double balance;
        char payload[64];

        if (!require_customer_session(session, response, response_size))
            return;

        if (!db_get_balance(db, session->customer_account_no, &balance,
                            error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        snprintf(payload, sizeof(payload), "%.2f", balance);
        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "DEPOSIT") == 0) {
        double new_balance;
        char payload[64];

        if (!require_customer_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_double(lines[1], &amount)) {
            make_response(response, response_size, "ERR",
                          "DEPOSIT requires a numeric amount.");
            return;
        }

        if (!db_deposit(db, session->customer_account_no, amount,
                        &new_balance, error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        snprintf(payload, sizeof(payload), "%.2f", new_balance);
        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "WITHDRAW") == 0) {
        double new_balance;
        char payload[64];

        if (!require_customer_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_double(lines[1], &amount)) {
            make_response(response, response_size, "ERR",
                          "WITHDRAW requires a numeric amount.");
            return;
        }

        if (!db_withdraw(db, session->customer_account_no, amount,
                         &new_balance, error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        snprintf(payload, sizeof(payload), "%.2f", new_balance);
        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "TRANSFER") == 0) {
        int receiver_account;
        double new_balance;
        char payload[64];

        if (!require_customer_session(session, response, response_size))
            return;

        if (line_count < 3
            || !parse_int(lines[1], &receiver_account)
            || !parse_double(lines[2], &amount)) {
            make_response(response, response_size, "ERR",
                          "TRANSFER requires receiver account number and amount.");
            return;
        }

        if (!db_transfer(db, session->customer_account_no,
                         receiver_account, amount, &new_balance,
                         error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        snprintf(payload, sizeof(payload), "%.2f", new_balance);
        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "MINI_STATEMENT") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];

        if (!require_customer_session(session, response, response_size))
            return;

        if (!db_build_mini_statement(db, session->customer_account_no,
                                     payload, sizeof(payload),
                                     error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "MONTHLY_SUMMARY") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];
        const char *month = line_count >= 2 ? lines[1] : "";

        if (!require_customer_session(session, response, response_size))
            return;

        if (!db_build_monthly_summary(db, session->customer_account_no,
                                      month, payload, sizeof(payload),
                                      error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "LIST_ACCOUNTS") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];

        if (!require_admin_session(session, response, response_size))
            return;

        if (!db_build_account_listing(db, payload, sizeof(payload),
                                      error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "VIEW_ACCOUNT_DETAILS") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_int(lines[1], &account_no)) {
            make_response(response, response_size, "ERR",
                          "VIEW_ACCOUNT_DETAILS requires an account number.");
            return;
        }

        if (!db_build_account_details(db, account_no, payload, sizeof(payload),
                                      error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "VIEW_ACCOUNT_STATEMENT") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_int(lines[1], &account_no)) {
            make_response(response, response_size, "ERR",
                          "VIEW_ACCOUNT_STATEMENT requires an account number.");
            return;
        }

        if (!db_build_mini_statement(db, account_no, payload, sizeof(payload),
                                     error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "VIEW_ACCOUNT_SUMMARY") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];
        const char *month = line_count >= 3 ? lines[2] : "";

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_int(lines[1], &account_no)) {
            make_response(response, response_size, "ERR",
                          "VIEW_ACCOUNT_SUMMARY requires an account number.");
            return;
        }

        if (!db_build_monthly_summary(db, account_no, month,
                                      payload, sizeof(payload),
                                      error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "ADMIN_DASHBOARD") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];

        if (!require_admin_session(session, response, response_size))
            return;

        if (!db_build_system_summary(db, payload, sizeof(payload),
                                     error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "RECENT_TRANSACTIONS") == 0) {
        char payload[ATM_MAX_MESSAGE_SIZE];
        int limit = 10;

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count >= 2 && !parse_int(lines[1], &limit)) {
            make_response(response, response_size, "ERR",
                          "RECENT_TRANSACTIONS limit must be numeric.");
            return;
        }

        if (!db_build_recent_transactions(db, limit, payload, sizeof(payload),
                                          error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "LOCK_ACCOUNT") == 0) {
        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_int(lines[1], &account_no)) {
            make_response(response, response_size, "ERR",
                          "LOCK_ACCOUNT requires an account number.");
            return;
        }

        if (!db_lock_account(db, account_no, error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", "Account locked.");
        return;
    }

    if (strcmp(lines[0], "UNLOCK_ACCOUNT") == 0) {
        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_int(lines[1], &account_no)) {
            make_response(response, response_size, "ERR",
                          "UNLOCK_ACCOUNT requires an account number.");
            return;
        }

        if (!db_unlock_account(db, account_no, error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", "Account unlocked.");
        return;
    }

    if (strcmp(lines[0], "ADMIN_CREDIT") == 0) {
        double new_balance;
        char payload[64];

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 3
            || !parse_int(lines[1], &account_no)
            || !parse_double(lines[2], &amount)) {
            make_response(response, response_size, "ERR",
                          "ADMIN_CREDIT requires account number and amount.");
            return;
        }

        if (!db_admin_credit(db, account_no, amount, &new_balance,
                             error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        snprintf(payload, sizeof(payload), "%.2f", new_balance);
        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "ADMIN_DEBIT") == 0) {
        double new_balance;
        char payload[64];

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 3
            || !parse_int(lines[1], &account_no)
            || !parse_double(lines[2], &amount)) {
            make_response(response, response_size, "ERR",
                          "ADMIN_DEBIT requires account number and amount.");
            return;
        }

        if (!db_admin_debit(db, account_no, amount, &new_balance,
                            error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        snprintf(payload, sizeof(payload), "%.2f", new_balance);
        make_response(response, response_size, "OK", payload);
        return;
    }

    if (strcmp(lines[0], "EDIT_ACCOUNT") == 0) {
        int new_account_no;

        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 5
            || !parse_int(lines[1], &account_no)
            || !parse_int(lines[2], &new_account_no)
            || !parse_int(lines[4], &pin)) {
            make_response(response, response_size, "ERR",
                          "EDIT_ACCOUNT requires current account number, "
                          "new account number, new name, and new 4-digit PIN.");
            return;
        }

        if (!db_edit_account(db, account_no, new_account_no,
                             lines[3], pin, error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK",
                      "Account updated successfully.");
        return;
    }

    if (strcmp(lines[0], "DELETE_ACCOUNT") == 0) {
        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2 || !parse_int(lines[1], &account_no)) {
            make_response(response, response_size, "ERR",
                          "DELETE_ACCOUNT requires an account number.");
            return;
        }

        if (!db_delete_account(db, account_no, error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK", "Account deleted.");
        return;
    }

    if (strcmp(lines[0], "CHANGE_ADMIN_PASSWORD") == 0) {
        if (!require_admin_session(session, response, response_size))
            return;

        if (line_count < 2) {
            make_response(response, response_size, "ERR",
                          "CHANGE_ADMIN_PASSWORD requires a new password.");
            return;
        }

        if (!db_change_admin_password(db, session->admin_username,
                                      lines[1], error, sizeof(error))) {
            make_response(response, response_size, "ERR", error);
            return;
        }

        make_response(response, response_size, "OK",
                      "Admin password updated.");
        return;
    }

    make_response(response, response_size, "ERR", "Unknown command.");
}

void handle_client(int client_fd, Database *db) {
    char request[ATM_MAX_MESSAGE_SIZE];
    char response[ATM_MAX_MESSAGE_SIZE];
    Session session;

    reset_session(&session);

    while (receive_message(client_fd, request, sizeof(request))) {
        handle_request(db, &session, request, response, sizeof(response));
        if (!send_message(client_fd, response)) {
            break;
        }
    }
}
