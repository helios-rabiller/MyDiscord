#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <libpq-fe.h>

#pragma comment(lib, "ws2_32.lib") // Pour Winsock

#define PORT 8080

void handle_client(SOCKET client_socket) {
    char buffer[1024] = {0};
    int recv_len = recv(client_socket, buffer, sizeof(buffer), 0);

    if (recv_len == SOCKET_ERROR) {
        perror("Erreur de réception de données du client");
        closesocket(client_socket);
        return;
    }

    char username[100], password[100];
    sscanf(buffer, "LOGIN:%s %s", username, password);

    PGconn *conn = PQconnectdb("dbname=MyDiscord user=postgres password=m");
    if (PQstatus(conn) != CONNECTION_OK) {
        perror("Erreur de connexion à la base de données");
        send(client_socket, "DB_ERROR", 8, 0);
        closesocket(client_socket);
        return;
    }

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT * FROM users WHERE username='%s' AND password_hash='%s'",
             username, password);

    PGresult *res = PQexec(conn, query);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        perror("Erreur lors de l'exécution de la requête");
        send(client_socket, "DB_ERROR", 8, 0);
        PQclear(res);
        PQfinish(conn);
        closesocket(client_socket);
        return;
    }

    if (PQntuples(res) == 1) {
        send(client_socket, "SUCCESS", 7, 0);
    } else {
        send(client_socket, "FAILURE", 7, 0);
    }

    PQclear(res);
    PQfinish(conn);
    closesocket(client_socket);
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        perror("Erreur Winsock");
        return 1;
    }

    SOCKET server_fd, client_socket;
    struct sockaddr_in server, client;
    int c = sizeof(struct sockaddr_in);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        perror("Erreur de création de la socket");
        WSACleanup();
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        perror("Erreur de binding de la socket");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    if (listen(server_fd, 3) == SOCKET_ERROR) {
        perror("Erreur d'écoute sur le port");
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }

    printf("Serveur en écoute sur le port %d...\n", PORT);

    while ((client_socket = accept(server_fd, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        printf("Connexion acceptée\n");
        handle_client(client_socket);
    }

    if (client_socket == INVALID_SOCKET) {
        perror("Erreur d'acceptation de la connexion");
    }

    closesocket(server_fd);
    WSACleanup();
    return 0;
}
