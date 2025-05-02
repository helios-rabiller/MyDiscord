#include <gtk/gtk.h>
#include <libpq-fe.h>
#include <locale.h>

// Fonction pour insérer une réaction dans la base de données
void add_reaction_to_db(int message_id, const char *reaction) {
    PGconn *conn = PQconnectdb("user=postgres dbname=MyDiscord password=m");
    if (PQstatus(conn) != CONNECTION_OK) {
        g_print("Connexion échouée : %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    char query[512];
    snprintf(query, sizeof(query),
             "UPDATE messages SET reactions = '%s' WHERE id = %d;",
             reaction, message_id);

    PGresult *res = PQexec(conn, query);
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        g_print("Erreur lors de l'ajout de la réaction : %s\n", PQerrorMessage(conn));
    }

    PQclear(res);
    PQfinish(conn);
}

// Callback lorsqu'une réaction est cliquée
void on_reaction_clicked(GtkButton *button, gpointer user_data) {
    const char *reaction = gtk_button_get_label(button);
    int message_id = GPOINTER_TO_INT(user_data);

    g_print("Ajout de la réaction '%s' au message ID %d\n", reaction, message_id);
    add_reaction_to_db(message_id, reaction);
}

// Fonction qui affiche la popup de réactions
void show_reaction_popup(GtkButton *button, gpointer user_data) {
    int message_id = GPOINTER_TO_INT(user_data);
    GtkWidget *parent = GTK_WIDGET(button);

    GtkWidget *popover = gtk_popover_new();
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_add_css_class(box, "reaction-box");

    const char *emojis[] = {"😀", "❤️", "👍", "🔥", "😢"};
    for (int i = 0; i < 5; ++i) {
        GtkWidget *btn = gtk_button_new_with_label(emojis[i]);
        gtk_widget_add_css_class(btn, "reaction-button");
        g_signal_connect(btn, "clicked", G_CALLBACK(on_reaction_clicked), GINT_TO_POINTER(message_id));
        gtk_box_append(GTK_BOX(box), btn);
    }

    gtk_popover_set_child(GTK_POPOVER(popover), box);
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), TRUE);
    gtk_popover_set_autohide(GTK_POPOVER(popover), TRUE);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);
    gtk_widget_set_parent(popover, parent);
    gtk_popover_popup(GTK_POPOVER(popover));

    gtk_widget_set_visible(popover, TRUE);
}

// Fonction pour récupérer les messages depuis la base de données
void fetch_messages(GtkWidget *box) {
    PGconn *conn = PQconnectdb("user=postgres dbname=MyDiscord password=m");
    if (PQstatus(conn) != CONNECTION_OK) {
        g_print("Connexion échouée : %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    PGresult *res = PQexec(conn, "SELECT id, content, timestamp, reactions FROM messages WHERE channel_id = '2' ORDER BY timestamp ASC");
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        g_print("Erreur de requête : %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return;
    }

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        int msg_id = atoi(PQgetvalue(res, i, 0));
        const char *content = PQgetvalue(res, i, 1);
        const char *timestamp = PQgetvalue(res, i, 2);
        const char *reaction = PQgetvalue(res, i, 3);

        GtkWidget *event_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
        gtk_widget_add_css_class(event_box, "message-box");

        GtkWidget *label = gtk_label_new(content);
        gtk_widget_add_css_class(label, "message-label");
        gtk_label_set_xalign(GTK_LABEL(label), 0.0);
        gtk_box_append(GTK_BOX(event_box), label);

        if (reaction && g_utf8_strlen(reaction, -1) > 0) {
            GtkWidget *reaction_label = gtk_label_new(reaction);
            gtk_widget_add_css_class(reaction_label, "reaction-label");
            gtk_label_set_xalign(GTK_LABEL(reaction_label), 0.0);
            gtk_box_append(GTK_BOX(event_box), reaction_label);
        }

        GtkWidget *ts_label = gtk_label_new(timestamp);
        gtk_widget_add_css_class(ts_label, "timestamp-label");
        gtk_label_set_xalign(GTK_LABEL(ts_label), 0.0);
        gtk_box_append(GTK_BOX(event_box), ts_label);

        GtkWidget *button = gtk_button_new();
        gtk_widget_add_css_class(button, "message-button");
        gtk_button_set_child(GTK_BUTTON(button), event_box);

        g_signal_connect(button, "clicked", G_CALLBACK(show_reaction_popup), GINT_TO_POINTER(msg_id));
        gtk_box_append(GTK_BOX(box), button);
    }

    PQclear(res);
    PQfinish(conn);
}

// Fonction de démarrage de l'application
static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_USER);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "MyDiscord");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    GtkWidget *msg_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_valign(msg_box, GTK_ALIGN_START);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), msg_box);
    gtk_window_set_child(GTK_WINDOW(window), scroll);

    fetch_messages(msg_box);
    gtk_window_present(GTK_WINDOW(window));
}

// Fonction principale
int main(int argc, char **argv) {
    setlocale(LC_ALL, "C.UTF-8");

    GtkApplication *app = gtk_application_new("com.example.MyDiscord", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
