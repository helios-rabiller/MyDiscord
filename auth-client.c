#include <gtk/gtk.h>
#include <libpq-fe.h>
#include <dotenv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080

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

    // Connexion à la base (libpq lit automatiquement les variables d'env)
    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        show_alert(window, "Connexion à la base de données échouée.");
        PQfinish(conn);
        return;
    }

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT * FROM users WHERE username='%s' AND password_hash='%s'",
             username, password);

    PGresult *res = PQexec(conn, query);

    if (PQntuples(res) == 1) {
        show_alert(window, "Connexion réussie !");
    } else {
        show_alert(window, "Nom d'utilisateur ou mot de passe incorrect.");
    }

    PQclear(res);
    PQfinish(conn);
}

void on_register_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *window = GTK_WINDOW(user_data);
    const char *username = gtk_editable_get_text(GTK_EDITABLE(entry_username));
    const char *email = gtk_editable_get_text(GTK_EDITABLE(entry_email));
    const char *password = gtk_editable_get_text(GTK_EDITABLE(entry_password));

    PGconn *conn = PQconnectdb("");
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

    if (PQresultStatus(res) == PGRES_COMMAND_OK) {
        show_alert(window, "Inscription réussie !");
    } else {
        show_alert(window, "Erreur lors de l'inscription. L'email est peut-être déjà utilisé.");
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

    env_load(".", false);

    int client_fd;
    struct sockaddr_in server_addr;
    // signal(SIGINT, handle_sigint);

    // 1. Créer le socket
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        perror("Erreur socket");
        exit(EXIT_FAILURE);
    }

    // 2. Définir adresse du serveur
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    // 3. Connexion
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erreur connexion");
        exit(EXIT_FAILURE);
    }

    printf("✅ Connecté au serveur sur le port %d\n", PORT);

    

    GtkApplication *app = gtk_application_new("com.exemple.AuthApp", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    close(client_fd);
    return status;
}
