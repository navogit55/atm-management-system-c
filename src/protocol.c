#include "protocol.h"

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int write_all(int socket_fd, const void *buffer, size_t length) {
    const char *cursor = (const char *) buffer;

    while (length > 0) {
        ssize_t written = send(socket_fd, cursor, length, 0);
        if (written <= 0) {
            return 0;
        }

        cursor += (size_t) written;
        length -= (size_t) written;
    }

    return 1;
}

static int read_all(int socket_fd, void *buffer, size_t length) {
    char *cursor = (char *) buffer;

    while (length > 0) {
        ssize_t received = recv(socket_fd, cursor, length, 0);
        if (received <= 0) {
            return 0;
        }

        cursor += (size_t) received;
        length -= (size_t) received;
    }

    return 1;
}

static int drain_bytes(int socket_fd, size_t length) {
    char discard[512];

    while (length > 0) {
        size_t chunk_size = length < sizeof(discard) ? length : sizeof(discard);
        if (!read_all(socket_fd, discard, chunk_size)) {
            return 0;
        }
        length -= chunk_size;
    }

    return 1;
}

int send_message(int socket_fd, const char *message) {
    uint32_t net_length;
    size_t length = 0U;

    if (message != NULL) {
        length = strlen(message);
    }

    if (length > UINT32_MAX) {
        return 0;
    }

    net_length = htonl((uint32_t) length);
    if (!write_all(socket_fd, &net_length, sizeof(net_length))) {
        return 0;
    }

    if (length == 0U) {
        return 1;
    }

    return write_all(socket_fd, message, length);
}

int receive_message(int socket_fd, char *buffer, size_t buffer_size) {
    uint32_t net_length;
    uint32_t length;

    if (buffer == NULL || buffer_size == 0U) {
        return 0;
    }

    if (!read_all(socket_fd, &net_length, sizeof(net_length))) {
        return 0;
    }

    length = ntohl(net_length);
    if ((size_t) length >= buffer_size) {
        if (!drain_bytes(socket_fd, (size_t) length)) {
            return 0;
        }
        return 0;
    }

    if (length > 0U && !read_all(socket_fd, buffer, (size_t) length)) {
        return 0;
    }

    buffer[length] = '\0';
    return 1;
}
