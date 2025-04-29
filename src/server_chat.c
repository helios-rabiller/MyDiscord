#include "server_chat.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dotenv.h>
#include <sys/socket.h> 


void querry_channel(char *buffer, int client_fd) {
    char *username = buffer + 5; 

    size_t len = strlen(username);
    if (len > 0 && username[len-1] == '\n') {
        username[len-1] = '\0';
    }

    env_load("..", false);

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "Erreur : %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    PGresult *res = PQexecParams(
        conn,
        "SELECT c.name FROM channels c JOIN user_roles ur ON c.id = ur.channel_id JOIN users u ON u.id = ur.user_id WHERE u.username = $1",
        1, NULL, (const char *[]) { username }, NULL, NULL, 0);
 
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Erreur requête SELECT : %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return;
    }

    int n = PQntuples(res);

    if (n == 0) {
        send(client_fd, "CHAN:END", strlen("CHAN:END"), 0);
        PQclear(res);
        PQfinish(conn);
        return;
    }

    char send_buffer[1024] = "CHAN:";

    for (int i = 0; i < n; i++) {
        const char *channel_name = PQgetvalue(res, i, 0);
        strcat(send_buffer, channel_name);
        if (i < n - 1) {
            strcat(send_buffer, "|");
        }
    }

    send(client_fd, send_buffer, strlen(send_buffer), 0);

    PQclear(res);
    PQfinish(conn);
}




querry_message(char *buffer, int client_fd){

    char *data = buffer + 5;
    char *saveptr;

    char *channel_name = strtok_r(data, "|", &saveptr);
    char *username = strtok_r(NULL, "|", &saveptr);
    char *content = strtok_r(NULL, "|", &saveptr);

    env_load("..", false);

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    const char *query = 
    "INSERT INTO messages (channel_id, user_id, content, is_encrypted) "
    "VALUES ("
    "(SELECT id FROM channels WHERE name = $1),"
    "(SELECT id FROM users WHERE username = $2),"
    "$3"
    ");";

    const char *params[3] = {channel_name , username, content};

    PGresult *res = PQexecParams(conn,query,3,NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        printf("message envoyé au canal '%s'\n", channel_name);
    } else {
        printf("Erreur lors de l'envoi du message : %s\n", PQerrorMessage(conn));
    }

    PQclear(res);
    PQfinish(conn);
}