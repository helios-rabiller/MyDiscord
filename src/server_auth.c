#include "server_auth.h"
#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dotenv.h>
#include <sys/socket.h>
#include <glib.h>

int check_email(char *email, PGconn *conn){

    char check_query[256];
    snprintf(check_query, sizeof(check_query),
            "SELECT id FROM users WHERE email = '%s'",email);

    PGresult *check_res = PQexec(conn, check_query);

    if (PQresultStatus(check_res) != PGRES_TUPLES_OK) {
        printf("Erreur lors de la vérification de l'email.\n");
        PQclear(check_res);
        return -1;
    }
    int exits = PQntuples(check_res) > 0;
    PQclear(check_res);
    return exits ? 1 : 0;
}

int check_user(char *username, PGconn *conn){

    char check_query[256];
    snprintf(check_query, sizeof(check_query),
            "SELECT id FROM users WHERE username = '%s'",username);

    PGresult *check_res = PQexec(conn, check_query);

    if (PQresultStatus(check_res) != PGRES_TUPLES_OK) {
        printf("Erreur lors de la vérification d'user.\n");
        PQclear(check_res);
        return -1;
    }
    int exits = PQntuples(check_res) > 0;
    PQclear(check_res);
    return exits ? 1 : 0;
}



void add_general_channel(char *email, PGconn *conn) {
    PGresult *user_res = PQexecParams(conn,
        "SELECT id FROM users WHERE email = $1",
        1, NULL, (const char *[]) { email }, NULL, NULL, 0);

    if (PQntuples(user_res) == 0) {
        printf("Utilisateur non trouvé pour l'email %s\n", email);
        PQclear(user_res);
        return;
    }

    char user_id[16];
    strcpy(user_id, PQgetvalue(user_res, 0, 0));
    PQclear(user_res);

    PGresult *channel_res = PQexec(conn, "SELECT id FROM channels WHERE name = 'Général'");
    if (PQntuples(channel_res) == 0) {
        printf("Canal 'Général' non trouvé\n");
        PQclear(channel_res);
        return;
    }

    char channel_id[16];
    strcpy(channel_id, PQgetvalue(channel_res, 0, 0));
    PQclear(channel_res);

    PGresult *role_res = PQexec(conn, "SELECT id FROM roles WHERE name = 'member'");
    if (PQntuples(role_res) == 0) {
        printf("Role 'member' non trouvé\n");
        PQclear(role_res);
        return;
    }

    char role_id[16];
    strcpy(role_id, PQgetvalue(role_res, 0, 0));
    PQclear(role_res);

    const char *params[3] = { user_id, role_id, channel_id };
    PGresult *link_res = PQexecParams(conn,
        "INSERT INTO user_roles (user_id, role_id, channel_id) VALUES ($1, $2, $3)",
        3, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(link_res) == PGRES_COMMAND_OK) {
        printf("Utilisateur ajouté au canal général\n");
    } else {
        printf("Erreur lors de l'ajout au canal général : %s\n", PQerrorMessage(conn));
    }

    PQclear(link_res);
}


void create_user(char *buffer, int client_fd){

    char *data = buffer + 5;
    char *saveptr;

    char *username = strtok_r(data, "|", &saveptr);
    char *email = strtok_r(NULL, "|", &saveptr);
    char *password = strtok_r(NULL, "|", &saveptr);
    
    if (username) g_strchomp(username);
    if (email) g_strchomp(email);
    if (password) g_strchomp(password);

    if (
        !username || strlen(username) == 0 || username[strlen(username) - 1] == ' ' ||
        !email    || strlen(email) == 0    || email[strlen(email) - 1]    == ' ' ||
        !password || strlen(password) == 0 || password[strlen(password) - 1] == ' '
    ) {
        printf("Requête invalide : champs vides.\n");
        send(client_fd, "AUTH:0", strlen("AUTH:0"), 0);
        return;
    }

    env_load("..", false);

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Connexion à la base de données échouée.");
        PQfinish(conn);
        return;
    }

    int email_check = check_email(email, conn);
    int user_check = check_user(username, conn);

    if (email_check == 1){
        printf("Email existe déjà.\n");
        send(client_fd, "AUTH:2", strlen("AUTH:2"),0);
    } else if (user_check == 1){
        printf("Utilisateur existe déjà");
        send(client_fd, "AUTH:4", strlen("AUTH:4"),0);
    } 
    else if (email_check == -1 || user_check == -1) {
        printf("Erreur lors de la vérification de l'email.\n");
        send(client_fd, "AUTH:0", strlen("AUTH:0"), 0);
        PQfinish(conn);
        return;
        
    }
    else{

        PGresult *res = PQexecParams(
            conn,
            "INSERT INTO users (username, email, password_hash) "
            "VALUES ($1, $2, crypt($3, gen_salt('bf')))",
            3,
            NULL,
            (const char *[]) { username, email, password },
            NULL,
            NULL,
            0
        );
     

    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        printf("Inscription réussie pour %s\n", username);

        add_general_channel(email, conn);

        send(client_fd, "AUTH:1", strlen("AUTH:1"),0);
    } else {
        printf("Requête invalide : champs vides.\n");
        send(client_fd, "AUTH:3", strlen("AUTH:3"),0);
    }

    PQclear(res);
}
    PQfinish(conn);
    return;
}

void connect_auth(char *buffer, int client_fd){


    char *data = buffer + 5;
    char *saveptr;

    char *username = strtok_r(data, "|", &saveptr);
    char *password = strtok_r(NULL, "|", &saveptr);

    if (!username || strlen(username) == 0 ||
    !password || strlen(password) == 0) {

    printf("Requête invalide : champs vides.\n");
    send(client_fd, "CONN:0", strlen("CONN:0"), 0);
    return;
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
        "SELECT id FROM users WHERE username = $1 AND password_hash = crypt($2, password_hash)",
        2,
        NULL,
        (const char *[]) { username, password },
        NULL,
        NULL,
        0
    );    

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        printf("Erreur dans la requête SQL.\n");
        send(client_fd, "CONN:0", strlen("CONN:0"), 0);
    } else if (PQntuples(res) == 1) {
        printf("Connexion réussie pour %s\n", username);


        PGresult *update = PQexecParams(conn,
            "UPDATE users SET status = 'online' WHERE username = $1",
            1, NULL, (const char *[]) {username} , NULL, NULL, 0);
        
            if (PQresultStatus(update) != PGRES_COMMAND_OK) {
                printf("Erreur lors de la mise à jour du status online.\n");
                PQclear(update);
                PQclear(res);
                PQfinish(conn);
                return;
            }

            PQclear(update);
            send(client_fd, "CONN:1", strlen("CONN:1"), 0);
    } else {
        printf("Identifiants incorrects pour %s\n", username);
        send(client_fd, "CONN:2", strlen("CONN:2"), 0);
    }
    

    PQclear(res);
    PQfinish(conn);
}