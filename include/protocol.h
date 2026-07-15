#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

int send_message(int socket_fd, const char *message);
int receive_message(int socket_fd, char *buffer, size_t buffer_size);

#endif
