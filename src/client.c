#include "network.h"
#include "protocol.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int request_server(int socket_fd, const char *request, char *payload, size_t payload_size) {
    char response[ATM_MAX_MESSAGE_SIZE];
    char status[16];
    const char *newline;
    size_t status_length;

    if (!send_message(socket_fd, request) || !receive_message(socket_fd, response, sizeof(response))) {
        fprintf(stderr, "Lost connection to server.\n");
        return -1;
    }

    newline = strchr(response, '\n');
    if (newline == NULL) {
        fprintf(stderr, "Received malformed response from server.\n");
        return -1;
    }

    status_length = (size_t) (newline - response);
    if (status_length >= sizeof(status)) {
        fprintf(stderr, "Received malformed response status.\n");
        return -1;
    }

    memcpy(status, response, status_length);
    status[status_length] = '\0';

    if (payload != NULL && payload_size > 0U) {
        snprintf(payload, payload_size, "%s", newline + 1);
    }

    return strcmp(status, "OK") == 0 ? 1 : 0;
}

static int create_account_flow(int socket_fd) {
    int account_no;
    int pin;
    char name[ATM_NAME_LENGTH];
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_int_stdin("Enter Account Number: ", &account_no) || account_no <= 0) {
        printf("Invalid account number.\n");
        return 1;
    }

    if (!read_required_text_stdin("Enter Name: ", name, sizeof(name))) {
        printf("Name cannot be empty.\n");
        return 1;
    }

    if (!read_int_stdin("Set 4-digit PIN: ", &pin)) {
        printf("Invalid PIN.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "CREATE_ACCOUNT\n%d\n%s\n%d", account_no, name, pin);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int customer_menu(int socket_fd, int account_no, const char *customer_name) {
    int choice = 0;
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];

    while (1) {
        printf("\n==== CUSTOMER MENU ====\n");
        printf("Logged in as %s (Acc:%d)\n", customer_name, account_no);
        printf("1. Check Balance\n");
        printf("2. Deposit\n");
        printf("3. Withdraw\n");
        printf("4. Transfer\n");
        printf("5. Monthly Transaction Summary\n");
        printf("6. Print Mini Statement\n");
        printf("7. Logout\n");

        if (!read_int_stdin("Choose: ", &choice)) {
            if (feof(stdin)) return 0;
            printf("Invalid menu option.\n");
            continue;
        }

        if (choice == 1) {
            int rc = request_server(socket_fd, "BALANCE", payload, sizeof(payload));
            if (rc < 0) {
                return 0;
            }

            printf(rc ? "Balance: %s\n" : "%s\n", payload);
        } else if (choice == 2) {
            double amount;
            int rc;

            if (!read_double_stdin("Enter amount to deposit: ", &amount)) {
                printf("Invalid amount.\n");
                continue;
            }

            snprintf(request, sizeof(request), "DEPOSIT\n%.2f", amount);
            rc = request_server(socket_fd, request, payload, sizeof(payload));
            if (rc < 0) {
                return 0;
            }

            printf(rc ? "Deposit successful. New balance: %s\n" : "%s\n", payload);
        } else if (choice == 3) {
            double amount;
            int rc;

            if (!read_double_stdin("Enter amount to withdraw: ", &amount)) {
                printf("Invalid amount.\n");
                continue;
            }

            snprintf(request, sizeof(request), "WITHDRAW\n%.2f", amount);
            rc = request_server(socket_fd, request, payload, sizeof(payload));
            if (rc < 0) {
                return 0;
            }

            printf(rc ? "Withdrawal successful. New balance: %s\n" : "%s\n", payload);
        } else if (choice == 4) {
            int receiver_account;
            double amount;
            int rc;

            if (!read_int_stdin("Enter Receiver Account Number: ", &receiver_account) || receiver_account <= 0) {
                printf("Invalid receiver account number.\n");
                continue;
            }

            if (!read_double_stdin("Enter Amount: ", &amount)) {
                printf("Invalid amount.\n");
                continue;
            }

            snprintf(request, sizeof(request), "TRANSFER\n%d\n%.2f", receiver_account, amount);
            rc = request_server(socket_fd, request, payload, sizeof(payload));
            if (rc < 0) {
                return 0;
            }

            printf(rc ? "Transfer successful. New balance: %s\n" : "%s\n", payload);
        } else if (choice == 5) {
            char month[16];
            int rc;

            if (!read_line_stdin("Enter month (YYYY-MM) or press Enter for current month: ", month, sizeof(month))) {
                printf("Invalid month.\n");
                continue;
            }

            snprintf(request, sizeof(request), "MONTHLY_SUMMARY\n%s", month);
            rc = request_server(socket_fd, request, payload, sizeof(payload));
            if (rc < 0) {
                return 0;
            }

            printf("%s\n", payload);
        } else if (choice == 6) {
            char filename[64];
            FILE *file;
            int rc = request_server(socket_fd, "MINI_STATEMENT", payload, sizeof(payload));

            if (rc < 0) {
                return 0;
            }

            if (!rc) {
                printf("%s\n", payload);
                continue;
            }

            printf("%s\n", payload);
            snprintf(filename, sizeof(filename), "mini_statement_%d.txt", account_no);
            file = fopen(filename, "w");
            if (file == NULL) {
                printf("Mini statement generated, but failed to write %s.\n", filename);
                continue;
            }

            fprintf(file, "%s\n", payload);
            fclose(file);
            printf("Mini statement written to %s\n", filename);
        } else if (choice == 7) {
            int rc = request_server(socket_fd, "LOGOUT", payload, sizeof(payload));
            if (rc < 0) {
                return 0;
            }

            printf("%s\n", payload);
            return 1;
        } else {
            printf("Invalid menu option.\n");
        }
    }
}

static int customer_login_flow(int socket_fd) {
    int account_no;
    int pin;
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_int_stdin("Enter Account Number: ", &account_no) || account_no <= 0) {
        printf("Invalid account number.\n");
        return 1;
    }

    if (!read_int_stdin("Enter PIN: ", &pin)) {
        printf("Invalid PIN.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "LOGIN\n%d\n%d", account_no, pin);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    if (!rc) {
        printf("%s\n", payload);
        return 1;
    }

    printf("Login successful. Welcome, %s.\n", payload);
    return customer_menu(socket_fd, account_no, payload);
}

static int admin_request_and_print(int socket_fd, const char *request) {
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc = request_server(socket_fd, request, payload, sizeof(payload));

    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int admin_request_for_account_and_print(int socket_fd, const char *command, const char *prompt) {
    int account_no;
    char request[ATM_MAX_MESSAGE_SIZE];

    if (!read_int_stdin(prompt, &account_no) || account_no <= 0) {
        printf("Invalid account number.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "%s\n%d", command, account_no);
    return admin_request_and_print(socket_fd, request);
}

static int admin_view_account_summary_flow(int socket_fd) {
    int account_no;
    char month[16];
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_int_stdin("Enter account number: ", &account_no) || account_no <= 0) {
        printf("Invalid account number.\n");
        return 1;
    }

    if (!read_line_stdin("Enter month (YYYY-MM) or press Enter for current month: ", month, sizeof(month))) {
        printf("Invalid month.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "VIEW_ACCOUNT_SUMMARY\n%d\n%s", account_no, month);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int admin_adjust_account_flow(int socket_fd, const char *command, const char *action_label) {
    int account_no;
    double amount;
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_int_stdin("Enter account number: ", &account_no) || account_no <= 0) {
        printf("Invalid account number.\n");
        return 1;
    }

    if (!read_double_stdin("Enter amount: ", &amount)) {
        printf("Invalid amount.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "%s\n%d\n%.2f", command, account_no, amount);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    if (rc) {
        printf("%s successful. New balance: %s\n", action_label, payload);
    } else {
        printf("%s\n", payload);
    }

    return 1;
}

static int admin_edit_account_flow(int socket_fd) {
    int current_account_no;
    int new_account_no;
    int new_pin;
    char new_name[ATM_NAME_LENGTH];
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_int_stdin("Enter current account number: ", &current_account_no) || current_account_no <= 0) {
        printf("Invalid current account number.\n");
        return 1;
    }

    if (!read_int_stdin("Enter new account number: ", &new_account_no) || new_account_no <= 0) {
        printf("Invalid new account number.\n");
        return 1;
    }

    if (!read_required_text_stdin("Enter new name: ", new_name, sizeof(new_name))) {
        printf("Name cannot be empty.\n");
        return 1;
    }

    if (!read_int_stdin("Enter new 4-digit PIN/password: ", &new_pin)) {
        printf("Invalid PIN.\n");
        return 1;
    }

    snprintf(
        request,
        sizeof(request),
        "EDIT_ACCOUNT\n%d\n%d\n%s\n%d",
        current_account_no,
        new_account_no,
        new_name,
        new_pin
    );
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int admin_delete_account_flow(int socket_fd) {
    int account_no;
    char confirm[16];
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_int_stdin("Enter account number to delete: ", &account_no) || account_no <= 0) {
        printf("Invalid account number.\n");
        return 1;
    }

    if (!read_line_stdin("Type DELETE to confirm: ", confirm, sizeof(confirm))) {
        printf("Delete cancelled.\n");
        return 1;
    }

    if (strcmp(confirm, "DELETE") != 0) {
        printf("Delete cancelled.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "DELETE_ACCOUNT\n%d", account_no);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int admin_recent_transactions_flow(int socket_fd) {
    char limit_text[16];
    int limit = 10;
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_line_stdin("Enter number of recent transactions (default 10): ", limit_text, sizeof(limit_text))) {
        printf("Invalid value.\n");
        return 1;
    }

    if (limit_text[0] != '\0' && !parse_int(limit_text, &limit)) {
        printf("Invalid limit.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "RECENT_TRANSACTIONS\n%d", limit);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int admin_change_password_flow(int socket_fd) {
    char new_password[64];
    char confirm_password[64];
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_required_text_stdin("Enter new admin password: ", new_password, sizeof(new_password))) {
        printf("Password cannot be empty.\n");
        return 1;
    }

    if (!read_required_text_stdin("Confirm new admin password: ", confirm_password, sizeof(confirm_password))) {
        printf("Password confirmation cannot be empty.\n");
        return 1;
    }

    if (strcmp(new_password, confirm_password) != 0) {
        printf("Passwords do not match.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "CHANGE_ADMIN_PASSWORD\n%s", new_password);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    printf("%s\n", payload);
    return 1;
}

static int admin_menu(int socket_fd) {
    int choice = 0;

    while (1) {
        printf("\n==== ADMIN MENU ====\n");
        printf("1. View All Accounts\n");
        printf("2. View Account Details\n");
        printf("3. View Customer Mini Statement\n");
        printf("4. View Customer Monthly Summary\n");
        printf("5. Lock Account\n");
        printf("6. Unlock Account\n");
        printf("7. Credit Account\n");
        printf("8. Debit Account\n");
        printf("9. Edit Account\n");
        printf("10. Delete Account\n");
        printf("11. Admin Dashboard\n");
        printf("12. Recent Transactions\n");
        printf("13. Change Admin Password\n");
        printf("14. Logout\n");

        if (!read_int_stdin("Choose: ", &choice)) {
            if (feof(stdin)) return 0;
            printf("Invalid menu option.\n");
            continue;
        }

        if (choice == 1) {
            if (!admin_request_and_print(socket_fd, "LIST_ACCOUNTS")) {
                return 0;
            }
        } else if (choice == 2) {
            if (!admin_request_for_account_and_print(socket_fd, "VIEW_ACCOUNT_DETAILS", "Enter account number: ")) {
                return 0;
            }
        } else if (choice == 3) {
            if (!admin_request_for_account_and_print(socket_fd, "VIEW_ACCOUNT_STATEMENT", "Enter account number: ")) {
                return 0;
            }
        } else if (choice == 4) {
            if (!admin_view_account_summary_flow(socket_fd)) {
                return 0;
            }
        } else if (choice == 5) {
            if (!admin_request_for_account_and_print(socket_fd, "LOCK_ACCOUNT", "Enter account number to lock: ")) {
                return 0;
            }
        } else if (choice == 6) {
            if (!admin_request_for_account_and_print(socket_fd, "UNLOCK_ACCOUNT", "Enter account number to unlock: ")) {
                return 0;
            }
        } else if (choice == 7) {
            if (!admin_adjust_account_flow(socket_fd, "ADMIN_CREDIT", "Credit")) {
                return 0;
            }
        } else if (choice == 8) {
            if (!admin_adjust_account_flow(socket_fd, "ADMIN_DEBIT", "Debit")) {
                return 0;
            }
        } else if (choice == 9) {
            if (!admin_edit_account_flow(socket_fd)) {
                return 0;
            }
        } else if (choice == 10) {
            if (!admin_delete_account_flow(socket_fd)) {
                return 0;
            }
        } else if (choice == 11) {
            if (!admin_request_and_print(socket_fd, "ADMIN_DASHBOARD")) {
                return 0;
            }
        } else if (choice == 12) {
            if (!admin_recent_transactions_flow(socket_fd)) {
                return 0;
            }
        } else if (choice == 13) {
            if (!admin_change_password_flow(socket_fd)) {
                return 0;
            }
        } else if (choice == 14) {
            if (!admin_request_and_print(socket_fd, "LOGOUT")) {
                return 0;
            }

            return 1;
        } else {
            printf("Invalid menu option.\n");
        }
    }
}

static int admin_login_flow(int socket_fd) {
    char username[64];
    char password[64];
    char request[ATM_MAX_MESSAGE_SIZE];
    char payload[ATM_MAX_MESSAGE_SIZE];
    int rc;

    if (!read_line_stdin("Enter admin username: ", username, sizeof(username))) {
        printf("Invalid username.\n");
        return 1;
    }

    if (!read_line_stdin("Enter admin password: ", password, sizeof(password))) {
        printf("Invalid password.\n");
        return 1;
    }

    snprintf(request, sizeof(request), "ADMIN_LOGIN\n%s\n%s", username, password);
    rc = request_server(socket_fd, request, payload, sizeof(payload));
    if (rc < 0) {
        return 0;
    }

    if (!rc) {
        printf("%s\n", payload);
        return 1;
    }

    printf("%s\n", payload);
    return admin_menu(socket_fd);
}

int main(int argc, char **argv) {
    const char *host = ATM_DEFAULT_HOST;
    int port = ATM_DEFAULT_PORT;
    int socket_fd;
    int choice = 0;

    if (argc >= 2) {
        host = argv[1];
    }

    if (argc >= 3 && !parse_int(argv[2], &port)) {
        fprintf(stderr, "Invalid port: %s\n", argv[2]);
        return 1;
    }

    socket_fd = connect_to_server(host, port);
    if (socket_fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        return 1;
    }

    printf("Connected to ATM server at %s:%d\n", host, port);

    while (1) {
        printf("\n==== MINI ATM CLIENT ====\n");
        printf("1. Create Account\n");
        printf("2. Customer Login\n");
        printf("3. Admin Login\n");
        printf("4. Exit\n");

        if (!read_int_stdin("Choose: ", &choice)) {
            if (feof(stdin)) break;
            printf("Invalid menu option.\n");
            continue;
        }

        if (choice == 1) {
            if (!create_account_flow(socket_fd)) break;
        } else if (choice == 2) {
            if (!customer_login_flow(socket_fd)) break;
        } else if (choice == 3) {
            if (!admin_login_flow(socket_fd)) break;
        } else if (choice == 4) {
            break;
        } else {
            printf("Invalid menu option.\n");
        }
    }

    close(socket_fd);
    printf("Disconnected from ATM server.\n");
    return 0;
}
