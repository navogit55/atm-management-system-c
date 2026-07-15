#include "auth.h"
#include "database.h"
#include "logger.h"
#include "network.h"
#include "utils.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

extern void handle_client(int client_fd, Database *db);

int main(int argc, char **argv) {
    int port = ATM_DEFAULT_PORT;
    const char *db_path = "data/atm.db";
    Database db;
    char error[256];
    int server_fd;

    if (argc >= 2 && !parse_int(argv[1], &port)) {
        fprintf(stderr, "Invalid port: %s\n", argv[1]);
        return 1;
    }

    if (argc >= 3) {
        db_path = argv[2];
    }

    if (!db_open(&db, db_path, error, sizeof(error))) {
        log_error("Failed to initialize database: %s", error);
        return 1;
    }

    if (!db_seed_admin(&db, error, sizeof(error))) {
        log_error("Failed to seed admin account: %s", error);
        db_close(&db);
        return 1;
    }

    server_fd = server_create_socket(port);
    if (server_fd < 0) {
        db_close(&db);
        return 1;
    }

    log_info("ATM server listening on port %d using %s", port, db_path);
    log_info("Set ATM_ADMIN_USERNAME and ATM_ADMIN_PASSWORD env vars for admin access");

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_length = sizeof(client_address);
        int client_fd = accept(server_fd,
                               (struct sockaddr *) &client_address,
                               &client_length);

        if (client_fd < 0) {
            log_error("Failed to accept client connection");
            continue;
        }

        log_info("Client connected from %s:%d",
                 inet_ntoa(client_address.sin_addr),
                 ntohs(client_address.sin_port));

        handle_client(client_fd, &db);
        close(client_fd);
        log_info("Client disconnected");
    }

    close(server_fd);
    db_close(&db);
    return 0;
}
