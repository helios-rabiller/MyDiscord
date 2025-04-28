
#include <signal.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <libpq-fe.h>
#include "app_chat.h"
#include "app_context.h"
#include <sys/socket.h> 

char buffer[512];

typedef struct {
    GtkWidget *chat_label;
    GtkWidget *channel_list;
    GtkWidget *chat_display;
    GtkWidget *entry;
    GtkWidget *user_list;
    AppContext *ctx;
} ChatContext;

static void load_messages(ChatContext *ctx, const char *channel_name) {
    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    char query[512];
    snprintf(query, sizeof(query),
        "SELECT sender, content FROM messages WHERE channel_id = (SELECT id FROM channels WHERE name = '%s')",
        channel_name);

    PGresult *res = PQexec(conn, query);
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->chat_display));
    gtk_text_buffer_set_text(buffer, "", -1);

    int n = PQntuples(res);
    for (int i = 0; i < n; i++) {
        const char *sender = PQgetvalue(res, i, 0);
        const char *content = PQgetvalue(res, i, 1);

        char message[512];
        snprintf(message, sizeof(message), "%s: %s\n", sender, content);
        gtk_text_buffer_insert_at_cursor(buffer, message, -1);
    }

    PQclear(res);
    PQfinish(conn);
}

static void on_channel_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    if (row != NULL) {
        ChatContext *ctx = (ChatContext *)user_data;
        GtkWidget *label = gtk_list_box_row_get_child(row);
        const char *channel_name = gtk_label_get_text(GTK_LABEL(label));

        char title[256];
        snprintf(title, sizeof(title), "Chat - %s", channel_name);
        gtk_label_set_text(GTK_LABEL(ctx->chat_label), title);

        load_messages(ctx, channel_name);
    }
}

static void load_channels(ChatContext *ctx) {

    
    snprintf(buffer, sizeof(buffer), "CHAN:%s", ctx->ctx->username);
    send(ctx->ctx->client_fd, buffer, strlen(buffer), 0);




    // for (int i = 0; i < n; i++) {
    //     const char *channel_name = PQgetvalue(res, i, 0);
    //     GtkWidget *label = gtk_label_new(channel_name);
    //     gtk_list_box_append(GTK_LIST_BOX(ctx->channel_list), label);
    // }


}

static void load_users(ChatContext *ctx) {
    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    PGresult *res = PQexec(conn, "SELECT username FROM users WHERE status = 'online'");
    int n = PQntuples(res);
    for (int i = 0; i < n; i++) {
        const char *username = PQgetvalue(res, i, 0);
        GtkWidget *label = gtk_label_new(username);
        gtk_list_box_append(GTK_LIST_BOX(ctx->user_list), label);
    }

    PQclear(res);
    PQfinish(conn);
}

static void on_send_message(GtkButton *button, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;
    const char *message = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));

    if (strlen(message) == 0) return;

    GtkListBoxRow *selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(ctx->channel_list));
    if (!selected_row) return;

    GtkWidget *label = gtk_list_box_row_get_child(selected_row);
    const char *channel_name = gtk_label_get_text(GTK_LABEL(label));

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    const char *paramValues[3] = { ctx->ctx->username, channel_name, message };

    PGresult *res = PQexecParams(conn,
        "INSERT INTO messages (sender, channel_id, content) VALUES ($1, (SELECT id FROM channels WHERE name = $2), $3)",
        3, NULL, paramValues, NULL, NULL, 0);

    PQclear(res);
    PQfinish(conn);

    gtk_editable_set_text(GTK_EDITABLE(ctx->entry), "");
    load_messages(ctx, channel_name);
}

void show_chat_window(GtkApplication *app, GtkWindow *previous_window, AppContext *global_ctx) {
    gtk_window_destroy(previous_window);

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Chat");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);

    GtkWidget *main_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_window_set_child(GTK_WINDOW(window), main_hbox);

    GtkWidget *vbox_left = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(vbox_left, 200, -1);
    gtk_box_append(GTK_BOX(main_hbox), vbox_left);

    GtkWidget *channel_list = gtk_list_box_new();
    gtk_box_append(GTK_BOX(vbox_left), channel_list);

    GtkWidget *chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(main_hbox), chat_box);

    GtkWidget *chat_label = gtk_label_new("Chat - Aucun canal");
    gtk_box_append(GTK_BOX(chat_box), chat_label);

    GtkWidget *chat_display = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_display), FALSE);
    gtk_box_append(GTK_BOX(chat_box), chat_display);

    GtkWidget *entry = gtk_entry_new();
    GtkWidget *btn_send = gtk_button_new_with_label("Envoyer");
    gtk_box_append(GTK_BOX(chat_box), entry);
    gtk_box_append(GTK_BOX(chat_box), btn_send);

    GtkWidget *vbox_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(vbox_right, 150, -1);
    gtk_box_append(GTK_BOX(main_hbox), vbox_right);

    GtkWidget *user_list = gtk_list_box_new();
    gtk_box_append(GTK_BOX(vbox_right), user_list);

    ChatContext *ctx = g_malloc(sizeof(ChatContext));
    ctx->chat_label = chat_label;
    ctx->channel_list = channel_list;
    ctx->chat_display = chat_display;
    ctx->entry = entry;
    ctx->user_list = user_list;
    ctx->ctx = global_ctx;

    load_channels(ctx);
    load_users(ctx);

    g_signal_connect(channel_list, "row-selected", G_CALLBACK(on_channel_selected), ctx);
    g_signal_connect(btn_send, "clicked", G_CALLBACK(on_send_message), ctx);

    g_signal_connect(window, "destroy", G_CALLBACK(g_free), ctx);

    gtk_window_present(GTK_WINDOW(window));
}
