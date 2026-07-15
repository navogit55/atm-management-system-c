#ifndef NETWORK_H
#define NETWORK_H

int server_create_socket(int port);
int connect_to_server(const char *host, int port);

#endif
