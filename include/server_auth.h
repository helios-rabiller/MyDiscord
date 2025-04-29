#ifndef SERVER_AUTH_H
#define SERVER_AUTH_H

void connect_auth(char *buffer, int client_fd);
void create_user(char *buffer, int client_fd);

#endif
