#include "server_chat.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dotenv.h>
#include <sys/socket.h> 
#include <time.h>


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


void querry_message(char *buffer, int client_fd){

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
    "INSERT INTO messages (channel_id, user_id, content) "
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

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M", t);

    char clean_msg[1024];
    strncpy(clean_msg, content, sizeof(clean_msg));
    clean_msg[sizeof(clean_msg) - 1] = '\0';
    for (char *p = clean_msg; *p; p++) {
        if (*p == '\n' || *p == '\r') *p = ' ';
}


    char formatted_msg[2000];
    snprintf(formatted_msg, sizeof(formatted_msg), "NEW:%s|%s|%s\n", username, clean_msg, timestamp);

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1) {
            send(clients[i], formatted_msg, strlen(formatted_msg), 0);
        }
    }
    

    pthread_mutex_unlock(&clients_mutex);
}

void send_channel_history(char *buffer, int client_fd) {
    char *channel_name = buffer + 5;

    printf("[SERVEUR] Envoi historique au client %d\n", client_fd);

    size_t len = strlen(channel_name);
    if (len > 0 && channel_name[len-1] == '\n') {
        channel_name[len-1] = '\0';
    }

    env_load("..", false);
    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        PQfinish(conn);
        return;
    }

    const char *param[1] = { channel_name };

    PGresult *res = PQexecParams(
        conn,
        "SELECT u.username, m.content, m.timestamp FROM messages m "
        "JOIN users u ON u.id = m.user_id "
        "WHERE m.channel_id = (SELECT id FROM channels WHERE name = $1) "
        "ORDER BY m.timestamp ASC",
        1, NULL, param, NULL, NULL, 0
    );


    
    if (PQresultStatus(res) == PGRES_TUPLES_OK) {
        int n = PQntuples(res);
        for (int i = 0; i < n; i++) {
            const char *username = PQgetvalue(res, i, 0);
            const char *message = PQgetvalue(res, i, 1);
            const char *timestamp = PQgetvalue(res, i, 2);
            char formatted[2000];

            char date[20];
            strncpy(date, timestamp, 16);
            date[16] = '\0';
            
            char clean_msg[1024];
            strncpy(clean_msg, message, sizeof(clean_msg));
            clean_msg[sizeof(clean_msg) - 1] = '\0';

            for (char *p = clean_msg; *p; p++) {
                if (*p == '\n' || *p == '\r') {
                    *p = ' ';
    }
}

            printf("[SERVEUR] HIST: %s|%s|%s\n", username, clean_msg, date);
            snprintf(formatted, sizeof(formatted), "HIST:%s|%s|%s\n", username, clean_msg, date);
            send(client_fd, formatted, strlen(formatted), 0);
            usleep(10000);
        }
    }

    PQclear(res);
    PQfinish(conn);
}
