
#include <gtk/gtk.h>
#include <stdlib.h>
#include "alert_dialog_utils.h"


static gboolean _show_alert_dialog(gpointer data) {
    AlertData *alert_data = (AlertData *)data;
    GtkWidget *parent = alert_data->parent;
    const char *message = alert_data->message;

    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Erreur");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(parent)));

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(message);
    gtk_box_append(GTK_BOX(content_area), label);

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog), ok_button, GTK_RESPONSE_OK);

    g_signal_connect_swapped(ok_button, "clicked", G_CALLBACK(gtk_window_destroy), dialog);

    gtk_window_present(GTK_WINDOW(dialog));
    g_free(alert_data);
    return FALSE;
}

void show_alert_dialog(GtkWidget *parent, const char *message) {
    AlertData *data = g_malloc(sizeof(AlertData));
    data->parent = parent;
    data->message = message;
    g_idle_add(_show_alert_dialog, data);
}
