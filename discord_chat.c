#include <gtk/gtk.h>
#include <libpq-fe.h>
#include <locale.h>

// Fonction pour récupérer les messages depuis la base de données
void fetch_messages(GtkWidget *box) {
    PGconn *conn = PQconnectdb("user=postgres dbname=MyDiscord password=m");
    if (PQstatus(conn) != CONNECTION_OK) {
        g_print("Connexion à la base de données échouée : %s\n", PQerrorMessage(conn));
        PQfinish(conn);
        return;
    }

    // Exécution de la requête pour obtenir les messages
    PGresult *res = PQexec(conn, "SELECT content, timestamp FROM messages ORDER BY timestamp ASC");
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

        // Si le message ou le timestamp est vide, afficher un message par défaut
        if (!content || !timestamp) {
            content = "Message invalide";
            timestamp = "Heure inconnue";
        }

        // Créer le label pour le contenu du message
        GtkWidget *label = gtk_label_new(content);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);  // Pour couper les longs textes si nécessaire
        gtk_box_append(GTK_BOX(box), label);

        // Créer le label pour le timestamp
        GtkWidget *time_label = gtk_label_new(timestamp);
        gtk_label_set_ellipsize(GTK_LABEL(time_label), PANGO_ELLIPSIZE_END);
        gtk_box_append(GTK_BOX(box), time_label);
    }

    PQclear(res);
    PQfinish(conn);
}

// Fonction appelée lors de l'activation de l'application
static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *scrollable;

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

    // Récupérer les messages depuis la base de données
    fetch_messages(box);

    // Afficher la fenêtre
    gtk_window_present(GTK_WINDOW(window));  // Remplace gtk_widget_show
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;

    // S'assurer que l'encodage est correct (UTF-8)
    setlocale(LC_ALL, "C.UTF-8");

    // Créer l'application GTK
    app = gtk_application_new("com.example.GtkApp", G_APPLICATION_DEFAULT_FLAGS);  // Remplace G_APPLICATION_FLAGS_NONE
    
    // Connecter l'activation de l'application à la fonction on_activate
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    
    // Exécuter l'application
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    // Libérer les ressources de l'application
    g_object_unref(app);

    return status;
}
