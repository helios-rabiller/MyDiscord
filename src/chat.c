
#include <gtk/gtk.h>
#include <stdio.h>
#include "chat.h"

void show_chat_window(GtkApplication *app, GtkWindow *previous_window, const char *username) {
    gtk_window_destroy(previous_window);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Chat");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);

    gtk_window_set_child(GTK_WINDOW(window), box);

    char welcome[100];
    snprintf(welcome, sizeof(welcome), "Bienvenue, %s !", username);

    GtkWidget *label = gtk_label_new(welcome);
    gtk_box_append(GTK_BOX(box), label);

    GtkWidget *entry = gtk_entry_new();
    GtkWidget *btn_send = gtk_button_new_with_label("Envoyer");

    gtk_box_append(GTK_BOX(box), entry);
    gtk_box_append(GTK_BOX(box), btn_send);

    g_signal_connect(btn_send, "clicked", G_CALLBACK(gtk_window_destroy), window); // Juste pour test

    gtk_window_present(GTK_WINDOW(window));
}
