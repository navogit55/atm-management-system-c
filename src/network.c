#include "network.h"
#include "logger.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int server_create_socket(int port) {
    int server_fd;
    int reuse = 1;
    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_error("Failed to create socket: %s", "socket");
        return -1;
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        log_error("Failed to set socket option: %s", "setsockopt");
        close(server_fd);
        return -1;
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short) port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) != 0) {
        log_error("Failed to bind to port %d: %s", port, "bind");
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 10) != 0) {
        log_error("Failed to listen on socket: %s", "listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int connect_to_server(const char *host, int port) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *cursor;
    char port_text[16];
    int socket_fd = -1;
    int rc;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    snprintf(port_text, sizeof(port_text), "%d", port);
    rc = getaddrinfo(host, port_text, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "Could not resolve %s:%s: %s\n", host, port_text, gai_strerror(rc));
        return -1;
    }

    for (cursor = result; cursor != NULL; cursor = cursor->ai_next) {
        socket_fd = socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

        if (connect(socket_fd, cursor->ai_addr, cursor->ai_addrlen) == 0) {
            break;
        }

        close(socket_fd);
        socket_fd = -1;
    }

    freeaddrinfo(result);
    return socket_fd;
}
