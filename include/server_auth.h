#ifndef SERVER_AUTH_H
#define SERVER_AUTH_H
#include "server_chat.h"

#define MAX_CLIENTS 100

extern int clients[MAX_CLIENTS];
extern char *client_channels[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;


void connect_auth(char *buffer, int client_fd);
void create_user(char *buffer, int client_fd);

#endif
