#ifndef SERVER_CHAT_H
#define SERVER_CHAT_H

#include <pthread.h>

#define MAX_CLIENTS 100

extern int clients[MAX_CLIENTS];
extern char *client_channels[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;

void querry_channel(char *buffer, int client_fd);
void querry_message(char *buffer, int client_fd);
void send_channel_history(char *buffer, int client_fd);

#endif
