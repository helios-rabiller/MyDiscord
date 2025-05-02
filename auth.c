#include <gtk/gtk.h>
#include <libpq-fe.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

GtkWidget *entry_username;
GtkWidget *entry_password;
GtkWidget *entry_email;

void show_alert(GtkWindow *parent, const char *message) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Alerte");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 250, 100);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_top(box, 20);
    gtk_widget_set_margin_bottom(box, 20);
    gtk_widget_set_margin_start(box, 20);
    gtk_widget_set_margin_end(box, 20);

    GtkWidget *label = gtk_label_new(message);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *button = gtk_button_new_with_label("OK");
    gtk_box_append(GTK_BOX(box), button);

    gtk_window_set_child(GTK_WINDOW(dialog), box);
    gtk_widget_set_visible(dialog, TRUE);

    g_signal_connect(button, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
}

void on_login_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    const char *username = gtk_editable_get_text(GTK_EDITABLE(entry_username));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(entry_password));

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        show_alert(window, "Erreur Winsock lors de l'initialisation.");
        return;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        show_alert(window, "Erreur de création de la socket.");
        WSACleanup();
        return;
    }

    struct sockaddr_in server;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(8080);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        show_alert(window, "Impossible de se connecter au serveur.");
        closesocket(sock);
        WSACleanup();
        return;
    }

    char message[256];
    snprintf(message, sizeof(message), "LOGIN:%s %s", username, password);
    if (send(sock, message, strlen(message), 0) == SOCKET_ERROR) {
        show_alert(window, "Erreur d'envoi du message.");
        closesocket(sock);
        WSACleanup();
        return;
    }

    char response[64] = {0};
    int recv_len = recv(sock, response, sizeof(response) - 1, 0);
    if (recv_len == SOCKET_ERROR) {
        show_alert(window, "Erreur de réception de la réponse du serveur.");
        closesocket(sock);
        WSACleanup();
        return;
    }

    if (strcmp(response, "SUCCESS") == 0) {
        show_alert(window, "Connexion réussie !");
    } else if (strcmp(response, "FAILURE") == 0) {
        show_alert(window, "Nom d'utilisateur ou mot de passe incorrect.");
    } else {
        show_alert(window, "Erreur inconnue.");
    }

    closesocket(sock);
    WSACleanup();
}

void on_register_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    const char *username = gtk_editable_get_text(GTK_EDITABLE(entry_username));
    const char *email = gtk_editable_get_text(GTK_EDITABLE(entry_email));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(entry_password));

    PGconn *conn = PQconnectdb("dbname=MyDiscord user=postgres password=m");
    if (PQstatus(conn) != CONNECTION_OK) {
        show_alert(window, "Connexion à la base de données échouée.");
        PQfinish(conn);
        return;
    }

    char query[512];
    snprintf(query, sizeof(query),
             "INSERT INTO users (username, email, password_hash) VALUES ('%s', '%s', '%s')",
             username, email, password);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        show_alert(window, "Erreur lors de l'inscription. L'email est peut-être déjà utilisé.");
    } else {
        show_alert(window, "Inscription réussie !");
    }

    PQclear(res);
    PQfinish(conn);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Login");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 250);

    GtkWidget *grid = gtk_grid_new();
    gtk_widget_set_margin_top(grid, 20);
    gtk_widget_set_margin_start(grid, 20);
    gtk_widget_set_margin_end(grid, 20);
    gtk_widget_set_margin_bottom(grid, 20);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_window_set_child(GTK_WINDOW(window), grid);

    entry_username = gtk_entry_new();
    entry_email = gtk_entry_new();
    entry_password = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(entry_password), FALSE);

    GtkWidget *btn_login = gtk_button_new_with_label("Se connecter");
    GtkWidget *btn_register = gtk_button_new_with_label("S'inscrire");

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Nom d'utilisateur:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_username, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Email:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_email, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Mot de passe:"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_password, 1, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), btn_login, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), btn_register, 0, 4, 2, 1);

    g_signal_connect(btn_login, "clicked", G_CALLBACK(on_login_clicked), window);
    g_signal_connect(btn_register, "clicked", G_CALLBACK(on_register_clicked), window);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.exemple.AuthApp", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
