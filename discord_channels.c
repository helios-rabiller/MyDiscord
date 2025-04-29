#include <gtk/gtk.h>
#include <libpq-fe.h>
#include <locale.h>
#include <glib.h>

// Fonction pour ignorer les caractères invalides
gchar *ignore_invalid_chars(const gchar *str) {
    GString *result = g_string_new("");  // Créer une nouvelle chaîne vide

    const gchar *p = str;
    while (*p) {
        if (g_unichar_isprint(*p)) {
            g_string_append_c(result, *p);  // Ajouter le caractère si c'est imprimable
        }
        p++;
    }

    return g_string_free(result, FALSE);  // Retourner la chaîne sans caractères invalides
}

void fetch_messages(GtkWidget *box, const char *channel_id) {
    PGconn *conn = PQconnectdb("user=postgres dbname=MyDiscord password=m");
    if (PQstatus(conn) != CONNECTION_OK) {
        g_print("Connexion à la base de données échouée : %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    // Modifiez la requête SQL pour filtrer par channel_id
    gchar *query = g_strdup_printf("SELECT content, timestamp FROM messages WHERE channel_id = '%s' ORDER BY timestamp ASC", channel_id);
    PGresult *res = PQexec(conn, query);
    g_free(query); // Libérer la mémoire allouée pour la requête

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        g_print("Erreur lors de l'exécution de la requête : %s\n", PQerrorMessage(conn));
        PQclear(res);
        PQfinish(conn);
        return;
    }

    // Récupérer les messages et les afficher dans l'interface
    int nRows = PQntuples(res);
    for (int i = 0; i < nRows; i++) {
        const char *content = PQgetvalue(res, i, 0);
        const char *timestamp = PQgetvalue(res, i, 1);

        if (!content || !timestamp) {
            content = "Message invalide";
            timestamp = "Heure inconnue";
        }

        // Ignorer les caractères invalides dans les messages
        gchar *clean_content = ignore_invalid_chars(content);
        gchar *clean_timestamp = ignore_invalid_chars(timestamp);

        GtkWidget *label = gtk_label_new(clean_content);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(box), label);

        GtkWidget *time_label = gtk_label_new(clean_timestamp);
        gtk_label_set_ellipsize(GTK_LABEL(time_label), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(box), time_label);

        g_free(clean_content);  // Libération mémoire
        g_free(clean_timestamp);  // Libération mémoire
    }

    PQclear(res);
    PQfinish(conn);
}

// Fonction appelée lors de l'activation de l'application
static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *scrollable;
    GtkWidget *entry;
    GtkWidget *button;

    // Créer une fenêtre
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Discord");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

    // Créer un conteneur Box pour afficher les messages
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // Ajouter un widget Scrollable pour permettre le défilement
    scrollable = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrollable), box);
    gtk_window_set_child(GTK_WINDOW(window), scrollable);

    // Créer un champ de saisie pour l'ID du canal
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Entrez l'ID du canal");
    gtk_box_append(GTK_BOX(box), entry);

    // Fonction pour charger les messages
    g_signal_connect(entry, "activate", G_CALLBACK(fetch_messages), box);

    // Créer un bouton pour actualiser les messages
    button = gtk_button_new_with_label("Afficher les messages");
    g_signal_connect(button, "clicked", G_CALLBACK(fetch_messages), box);
    gtk_box_append(GTK_BOX(box), button);

    // Afficher la fenêtre
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    setlocale(LC_ALL, "C.UTF-8");

    app = gtk_application_new("com.example.GtkApp", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);

    return status;
}
