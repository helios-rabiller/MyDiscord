#include "server_chat.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dotenv.h>


void querry_channel(char *buffer, int client_fd){


char *data = buffer + 5;
char *saveptr;

char *username = strtok_r(data, "|", &saveptr);


env_load("..", false);

PGconn *conn = PQconnectdb("");
if (PQstatus(conn) != CONNECTION_OK) {
    printf("Erreur connexion DB\n");
    PQfinish(conn);
    return;
}

PGresult *res = PQexec(conn, "SELECT name FROM channels");


PQclear(res);
PQfinish(conn);

}