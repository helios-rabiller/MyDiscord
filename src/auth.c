
    #include <gtk/gtk.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <signal.h>
    #include <libpq-fe.h>
    #include <dotenv.h>
    #include "auth.h"
    #include "chat.h"
    #include "app_context.h"

    static GtkWidget *entry_username;
    static GtkWidget *entry_password;
    static GtkWidget *entry_email;

    static void on_alert_ok_clicked(GtkButton *button, gpointer user_data) {
        GtkWindow *dialog = GTK_WINDOW(user_data);
        gtk_window_destroy(dialog); 
    }

    void show_alert(GtkWindow *parent, const char *message) {
        GtkWidget *dialog = gtk_application_window_new(gtk_window_get_application(parent));
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
        gtk_window_present(GTK_WINDOW(dialog));

        g_signal_connect(button, "clicked", G_CALLBACK(on_alert_ok_clicked), dialog);
    }

    static void on_login_clicked(GtkButton *button, gpointer user_data) {
        GtkWindow *window = GTK_WINDOW(user_data);
        AppContext *ctx = (AppContext *)g_object_get_data(G_OBJECT(window), "ctx");

        printf("login cliked");
        const char *username = gtk_editable_get_text(GTK_EDITABLE(entry_username));
        const char *password = gtk_editable_get_text(GTK_EDITABLE(entry_password));
        printf("%s,%s",username,password);
        if (strlen(username) == 0 || strlen(password) == 0) {
            show_alert(window, "Tous les champs sont obligatoires.");
            return;
        }

        char buffer[512];
        snprintf(buffer, sizeof(buffer), "CONN : %s|%s", username, password);
        send(ctx->client_fd, buffer, strlen(buffer), 0);

        char auth_status[8];
        memset(auth_status, 0, sizeof(auth_status));

        int bytes_read = read(ctx->client_fd, auth_status, sizeof(auth_status) - 1);
        auth_status[bytes_read] = '\0';

        if (strcmp(auth_status, "CONN:1") == 0) {
            show_alert(window, "Connexion réussie !");
            strncpy(ctx->username, username, sizeof(ctx->username) - 1);
            ctx->username[sizeof(ctx->username) - 1] = '\0';
            show_chat_window(gtk_window_get_application(window), window, ctx->username);
        } else if (strcmp(auth_status, "CONN:0") == 0) {
            show_alert(window, "Erreur lors de l'inscription. Erreur inconnue.");
        } else if (strcmp(auth_status, "CONN:2") == 0) {
            show_alert(window, "Erreur lors de l'inscription. Champs vide.");
        } 
    }

    static void on_register_clicked(GtkButton *button, gpointer user_data) {
        GtkWindow *window = GTK_WINDOW(user_data);
        AppContext *ctx = (AppContext *)g_object_get_data(G_OBJECT(window), "ctx");

        const char *username = gtk_editable_get_text(GTK_EDITABLE(entry_username));
        const char *email = gtk_editable_get_text(GTK_EDITABLE(entry_email));
        const char *password = gtk_editable_get_text(GTK_EDITABLE(entry_password));

        if (strlen(username) == 0 || strlen(email) == 0 || strlen(password) == 0) {
            show_alert(window, "Tous les champs sont obligatoires.");
            return;
        }

        char buffer[512];
        snprintf(buffer, sizeof(buffer), "AUTH : %s|%s|%s", username, email, password);
        send(ctx->client_fd, buffer, strlen(buffer), 0);

        char auth_status[8];
        memset(auth_status, 0, sizeof(auth_status));

        int bytes_read = read(ctx->client_fd, auth_status, sizeof(auth_status) - 1);
        auth_status[bytes_read] = '\0';

        if (strcmp(auth_status, "AUTH:1") == 0) {
            show_alert(window, "Inscription réussie !");
        } else if (strcmp(auth_status, "AUTH:0") == 0) {
            show_alert(window, "Erreur lors de l'inscription. Erreur inconnue.");
        } else if (strcmp(auth_status, "AUTH:3") == 0) {
            show_alert(window, "Erreur lors de l'inscription. Champs vide.");
        } else if (strcmp(auth_status, "AUTH:2") == 0) {
            show_alert(window, "Erreur lors de l'inscription. L'email est déjà utilisé.");
        }
    }

    void show_auth_window(GtkApplication *app, gpointer user_data) {
        AppContext *ctx = (AppContext *)user_data;

        GtkWidget *window = gtk_application_window_new(app);
        gtk_window_set_title(GTK_WINDOW(window), "Login");
        gtk_window_set_default_size(GTK_WINDOW(window), 300, 250);

        g_object_set_data(G_OBJECT(window), "ctx", ctx);

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
