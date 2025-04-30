#include "server_chat.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dotenv.h>
#include <sys/socket.h> 
#include <time.h>
#include <glib.h>


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

void create_channel(char *buffer, int client_fd){
    char *data = buffer + 12;
    char *saveptr;

    char *channel_name = strtok_r(data, "|", &saveptr);
    char *username = strtok_r(NULL, "|", &saveptr);
    char *private = strtok_r(NULL, "|", &saveptr);

    if (channel_name) g_strchomp(channel_name);
    if (username) g_strchomp(username);
    if (private) g_strchomp(private);

    if (!channel_name || strlen(channel_name) == 0 || channel_name[strlen(channel_name) - 1] == ' ') {
        printf("Nom de canal invalide (vide ou se termine par un espace).\n");
        send(client_fd, "CREATE_CHAN:0", strlen("CREATE_CHAN:0"), 0);
        return;
    }

    bool is_private = (strcmp(private, "1") == 0);

    env_load("..", false);

    

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    PGresult *check = PQexecParams(conn,
        "SELECT COUNT(*) FROM channels WHERE name = $1",
        1, NULL, (const char *[]) { channel_name }, NULL, NULL, 0);
    
    if (PQresultStatus(check) != PGRES_TUPLES_OK) {
        printf("Erreur vérification canal : %s\n", PQerrorMessage(conn));
        PQclear(check);
        PQfinish(conn);
        return;
    }
    
    int exists = atoi(PQgetvalue(check, 0, 0));
    PQclear(check);
    
    if (exists > 0) {
        printf("Le canal '%s' existe déjà.\n", channel_name);
        send(client_fd, "CREATE_CHAN:EXISTS", strlen("CREATE_CHAN:EXISTS"), 0);
        PQfinish(conn);
        return;
    }
    

    const char *query = 
    "WITH new_channel AS ( "
    "INSERT INTO channels (name,is_private ,created_by) "
    "VALUES ($1,$2,(SELECT id FROM users WHERE username = $3)) "
    "RETURNING id) "
    "INSERT INTO user_roles (user_id,role_id,channel_id) "
    "VALUES ((SELECT id FROM users WHERE username = $4), "
            "(SELECT id FROM roles WHERE name = 'admin'), "
            "(SELECT id FROM new_channel)); "
            ;

    char is_private_str[2];
    snprintf(is_private_str, sizeof(is_private_str), "%d", is_private);
    const char *params[4] = {channel_name, is_private_str, username, username};

    PGresult *res = PQexecParams(conn,query,4,NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        printf("canal '%s' envoyé sur bdd\n", channel_name);
    } else {
        printf("Erreur lors de l'envoi du message : %s\n", PQerrorMessage(conn));
    }

    PQclear(res);
    PQfinish(conn);

    char notif[512];
    snprintf(notif, sizeof(notif), "NEWCHAN:%s\n", channel_name);
    
    send(client_fd, notif, strlen(notif), 0);

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

            snprintf(formatted, sizeof(formatted), "HIST:%s|%s|%s\n", username, clean_msg, date);
            send(client_fd, formatted, strlen(formatted), 0);
            usleep(10000);
        }
    }

    PQclear(res);
    PQfinish(conn);
}


void add_member(char *buffer, int client_fd) {
    char *data = buffer + 11;
    char *saveptr;

    char *channel_name = strtok_r(data, "|", &saveptr);
    char *username = strtok_r(NULL, "|", &saveptr);
    char *role = strtok_r(NULL, "|", &saveptr);

    if (username) g_strchomp(username);
    if (role) g_strchomp(role);

    if (!username || strlen(username) == 0 || username[strlen(username) - 1] == ' ') {
        printf("[ADD_MEMBER] username invalide.\n");
        send(client_fd, "ADD_MEMBER:0", strlen("ADD_MEMBER:0"), 0);
        return;
    }

    env_load("..", false);
    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    PGresult *check_user = PQexecParams(conn,
        "SELECT id FROM users WHERE username = $1",
        1, NULL, (const char *[]){ username }, NULL, NULL, 0);
    
    if (PQntuples(check_user) == 0) {
        send(client_fd, "ADD_MEMBER:UNKNOWN", strlen("ADD_MEMBER:UNKNOWN"), 0);
        PQclear(check_user);
        PQfinish(conn);
        return;
    }

    const char *params[3] = { username, role, channel_name };
    PGresult *res = PQexecParams(conn,
        "INSERT INTO user_roles (user_id, role_id, channel_id) VALUES ("
        "(SELECT id FROM users WHERE username = $1),"
        "(SELECT id FROM roles WHERE name = $2),"
        "(SELECT id FROM channels WHERE name = $3))",
        3, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        printf("Erreur INSERT : %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQclear(check_user);
        PQfinish(conn);
        return;
    }

    PQclear(res);

    char notif[512];
    snprintf(notif, sizeof(notif), "ADD_MEMBER:%s\n", channel_name);

    int user_id = atoi(PQgetvalue(check_user, 0, 0));
    PQclear(check_user);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] != -1 && client_user_ids[i] == user_id) {
            send(clients[i], notif, strlen(notif), 0);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    send(client_fd, "ADD_MEMBER:OK", strlen("ADD_MEMBER:OK"), 0);

    PQfinish(conn);
}


void remove_member(char *buffer, int client_fd) {



}