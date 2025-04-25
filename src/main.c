#include <gtk/gtk.h>
#include <stdio.h>              // printf(), perror() → affichage, messages d’erreur
#include <stdlib.h>             // exit(), malloc() → fonctions système et mémoire
#include <string.h>             // memset(), strlen(), strcpy(), strcmp(), strcspn()
#include <unistd.h>             // close(), read(), write() → gestion des fichiers/sockets
#include <arpa/inet.h>          // inet_pton(), htons(), sockaddr_in → IP / ports
#include <sys/socket.h>         // socket(), connect(), bind(), listen(), accept(), send(), recv()
#include <signal.h>             // signal(), SIGINT → gestion des signaux système (Ctrl+C, interruption)


#include "auth.h"
#include "app_context.h"

int main(int argc, char **argv) {
    AppContext ctx;

    ctx.client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx.client_fd < 0) {
        perror("Socket");
        return 1;
    }

    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(8080),
    };
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(ctx.client_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connexion serveur");
        return 1;
    }

    GtkApplication *app = gtk_application_new("com.app.chat", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(show_auth_window), &ctx);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    close(ctx.client_fd);
    return status;
}
//     struct sockaddr_in server_addr;

//     client_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (client_fd == -1) {
//         perror("Erreur socket");
//         exit(EXIT_FAILURE);
//     }

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

//     if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         perror("Erreur connexion");
//         exit(EXIT_FAILURE);
//     }

//     printf("Connecté au serveur sur le port %d\n", PORT);

//     close(client_fd);


//     struct sockaddr_in server_addr;

//     client_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (client_fd == -1) {
//         perror("Erreur socket");
//         exit(EXIT_FAILURE);
//     }

//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

//     if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
//         perror("Erreur connexion");
//         exit(EXIT_FAILURE);
//     }

//     printf("Connecté au serveur sur le port %d\n", PORT);

//     close(client_fd);