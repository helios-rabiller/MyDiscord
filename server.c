#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>      // inet_ntoa(), htons()
#include <pthread.h>        // threads POSIX
#include <signal.h>         // gestion Ctrl+C
#include <netinet/in.h>     // struct sockaddr_in
#include <libpq-fe.h>
#include <dotenv.h>


#define PORT 8080
#define MAX_CLIENTS 100

void create_user(char *buffer, int client_fd);


int server_fd; 


int clients[MAX_CLIENTS];               
int nb_clients = 0;                     
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; 


void handle_sigint(int sig) {
    (void)sig;
    printf("\n[Ctrl+C] Fermeture du serveur...\n");
    if (server_fd != -1) {
        close(server_fd);
        printf("Socket serveur fermé.\n");
    }
    exit(0);
}

// Fonction qui gère la communication avec un client
void *handle_client(void *arg) {
    int client_fd = *(int *)arg;   
    free(arg);                     
    char buffer[1024];

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++){
        if (clients[i] == -1){
            clients[i] = client_fd;
            nb_clients++;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);


    while (1) {
        memset(buffer, 0, sizeof(buffer));

        int bytes_read = read(client_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            printf("Client %d déconnecté.\n", client_fd);
            break;
        }

        buffer[strcspn(buffer, "\n")] = 0; 
        strcat(buffer, "\n");             

        char message[1050]; // pour mettre "Client X > " + buffer
        snprintf(message, sizeof(message), "Client %d > %s\n", client_fd, buffer);
    
        printf("Message reçu de %d : %s\n", client_fd, buffer);

        // Gestion des messages
        if (strncmp(buffer, "AUTH :", 6) == 0) {
            create_user(buffer,client_fd);
            continue;

        } else {    
                pthread_mutex_lock(&clients_mutex);

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] != -1 && clients[i] != client_fd) {
                        send(clients[i], message, strlen(message), 0);
                    }
                }

                pthread_mutex_unlock(&clients_mutex);
        }
    }

    close(client_fd);  // Fermer la connexion avec ce client

    pthread_mutex_lock(&clients_mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client_fd) {
            clients[i] = -1;
            nb_clients--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);

    return NULL;
}

int main() {
    struct sockaddr_in server_addr;

    // Gérer interruption clavier (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // 1. Créer le socket serveur (TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    // 2. Définir les infos du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // 3. Lier le socket à l’adresse
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur bind");
        exit(EXIT_FAILURE);
    }

    // 4. Mettre le socket en écoute
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Erreur listen");
        exit(EXIT_FAILURE);
    }

    printf("Serveur lancé sur le port %d. En attente de connexions...\n", PORT);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i] = -1;
    }

    // Boucle principale
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        // 5. Accepter une connexion
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("Erreur accept");
            continue;
        }

        // 6. Afficher IP + port
        char *ip = inet_ntoa(client_addr.sin_addr);
        int port = ntohs(client_addr.sin_port);
        printf("Nouveau client connecté depuis %s:%d\n", ip, port);

        // 7. Allouer dynamiquement le socket pour le thread
        int *pclient = malloc(sizeof(int));
        if (pclient == NULL) {
            perror("Erreur malloc");
            close(client_fd);
            continue;
        }
        *pclient = client_fd;

        // 8. Créer un thread pour gérer ce client
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, pclient) != 0) {
            perror("Erreur création thread");
            close(client_fd);
            free(pclient);
            continue;
        }

        // 9. Détacher le thread (pas besoin de pthread_join)
        pthread_detach(tid);
    }

    close(server_fd); 
    return 0;
}

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


void create_user(char *buffer, int client_fd){

    char *data = buffer + 6;
    while (*data == ' ') data++;
    char *saveptr;

    char *username = strtok_r(data, "|", &saveptr);
    char *email = strtok_r(NULL, "|", &saveptr);
    char *password = strtok_r(NULL, "|", &saveptr);

    if (!username || strlen(username) == 0 ||
    !email || strlen(email) == 0 ||
    !password || strlen(password) == 0) {

    printf("Requête invalide : champs vides.\n");
    send(client_fd, "AUTH:0", strlen("AUTH:0"), 0);
    return;
    }
    env_load(".", false);

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Connexion à la base de données échouée.");
        PQfinish(conn);
        return;
    }

    int check = check_email(email, conn);

    if (check == 1){
        printf("Email existe déjà.\n");
        send(client_fd, "AUTH:2", strlen("AUTH:2"),0);
    } else if (check == -1) {
        printf("Erreur lors de la vérification de l'email.\n");
        send(client_fd, "AUTH:0", strlen("AUTH:0"), 0);
        PQfinish(conn);
        return;
    }
    else{
    char query[512];
    snprintf(query, sizeof(query),
            "INSERT INTO users (username, email, password_hash) VALUES ('%s', '%s', '%s')",
            username, email, password);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        printf("Inscription réussie pour %s\n", username);
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

