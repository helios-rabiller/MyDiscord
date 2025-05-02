
#include <signal.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <libpq-fe.h>
#include "app_chat.h"
#include "app_context.h"
#include <sys/socket.h> 
#include "alert_dialog_utils.h"

#include <dotenv.h>



typedef struct {
    GtkWidget *chat_label;
    GtkWidget *channel_list;
    GtkWidget *chat_display;
    GtkWidget *entry;
    GtkWidget *connected_list;
    GtkWidget *disconnected_list;
    AppContext *ctx;
    char send_buffer[512];
    char selected_channel[128];
} ChatContext;

gboolean alert_create_channel(gpointer data, const char *message) {
    ChatContext *ctx = (ChatContext *)data;

    GtkWidget *dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Erreur");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_root(ctx->chat_label)));

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new(message);
    gtk_box_append(GTK_BOX(content_area), label);

    GtkWidget *ok_button = gtk_button_new_with_label("OK");
    gtk_dialog_add_action_widget(GTK_DIALOG(dialog), ok_button, GTK_RESPONSE_OK);

    g_signal_connect_swapped(ok_button, "clicked", G_CALLBACK(gtk_window_destroy), dialog);

    gtk_window_present(GTK_WINDOW(dialog));
    return FALSE; 
}


static gpointer listen_for_messages(gpointer user_data) {

    ChatContext *ctx = (ChatContext *)user_data;

    char recv_buffer[2048];

    while (1) {
        memset(recv_buffer, 0, sizeof(recv_buffer));
        int bytes_read = recv(ctx->ctx->client_fd, recv_buffer, sizeof(recv_buffer) - 1, 0);

        if (bytes_read <= 0) {
            printf("Déconnecté du serveur ou erreur.\n");
            break;
        }

        recv_buffer[bytes_read] = '\0';

        char *saveptr;
        char *line = strtok_r(recv_buffer, "\n", &saveptr);
        while (line != NULL) {

            if (g_str_has_prefix(line, "HIST:") || g_str_has_prefix(line, "NEW:")) {
                char *prefix_save;
                char *type = strtok_r(line, ":", &prefix_save);
                char *username = strtok_r(NULL, "|", &prefix_save);
                char *message = strtok_r(NULL, "|", &prefix_save);
                char *timestamp = strtok_r(NULL, "|", &prefix_save);
        
                if (username && message && timestamp) {
                    g_strchomp(username);
                    g_strchomp(message);
                    g_strchomp(timestamp);
        
                    char formatted[1024];
                    snprintf(formatted, sizeof(formatted), "[%s] %s : %s\n", timestamp, username, message);
                    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->chat_display));
                    gtk_text_buffer_insert_at_cursor(buffer, formatted, -1);
                }
            } else if (g_str_has_prefix(line, "NEWCHAN:")) {
                const char *channel_name = line + strlen("NEWCHAN:");
                
                gboolean exists = FALSE;
                for (GtkWidget *child = gtk_widget_get_first_child(ctx->channel_list);
                     child != NULL;
                     child = gtk_widget_get_next_sibling(child)) {
            
                    const char *existing_name = g_object_get_data(G_OBJECT(child), "channel_name");
                    if (existing_name && strcmp(existing_name, channel_name) == 0) {
                        exists = TRUE;
                        break;
                    }
                }
            
            
                if (!exists) {
                    GtkWidget *label = gtk_label_new(channel_name);
                    GtkWidget *row = gtk_list_box_row_new();
                    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
                    g_object_set_data(G_OBJECT(row), "channel_name", g_strdup(channel_name));
                    gtk_list_box_append(GTK_LIST_BOX(ctx->channel_list), row);
                    gtk_widget_show(row);
                }
            }

            else if (g_str_has_prefix(line, "ADD_MEMBER:OK")) {
                const char *channel_name = line + strlen("ADD_MEMBER:OK");
            
                gboolean exists = FALSE;
                for (GtkWidget *child = gtk_widget_get_first_child(ctx->channel_list);
                     child != NULL;
                     child = gtk_widget_get_next_sibling(child)) {
            
                    const char *existing_name = g_object_get_data(G_OBJECT(child), "channel_name");
                    if (existing_name && strcmp(existing_name, channel_name) == 0) {
                        exists = TRUE;
                        break;
                    }
                }
            
                if (!exists) {
                    GtkWidget *label = gtk_label_new(channel_name);
                    GtkWidget *row = gtk_list_box_row_new();
                    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
                    g_object_set_data(G_OBJECT(row), "channel_name", g_strdup(channel_name));
                    gtk_list_box_append(GTK_LIST_BOX(ctx->channel_list), row);
                    gtk_widget_show(row);
                }
            }
            
            else if (g_strcmp0(line, "ADD_MEMBER:0") == 0) {
                show_alert_dialog(ctx->chat_label, "Entrée invalide");
            }
            else if (g_strcmp0(line, "ADD_MEMBER:UNKNOWN") == 0) {
                show_alert_dialog(ctx->chat_label, "Utilisateur inconnu");
            }

            else if (g_strcmp0(line, "CREATE_CHAN:EXISTS") == 0) {
                show_alert_dialog(ctx->chat_label, "Ce canal existe déjà, merci d'en créer un autre");
            }

            else {
                g_print("Inconnu ou ignoré : %s\n", line);
            }
        
            line = strtok_r(NULL, "\n", &saveptr);
        }
}
return NULL;
}


static void on_create_channel_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;

    GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "entry");
    GtkWidget *check_button = g_object_get_data(G_OBJECT(dialog), "check");

    if (response_id == GTK_RESPONSE_OK) {
        const char *channel_name = gtk_editable_get_text(GTK_EDITABLE(entry));
        gboolean is_private = gtk_check_button_get_active(GTK_CHECK_BUTTON(check_button));

        if (strlen(channel_name) > 0) {
            snprintf(ctx->send_buffer, sizeof(ctx->send_buffer), "CREATE_CHAN:%s|%s|%d", channel_name, ctx->ctx->username, is_private ? 1 : 0);
            send(ctx->ctx->client_fd, ctx->send_buffer, strlen(ctx->send_buffer), 0);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}


static void on_create_channel(GtkButton *button, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Créer un canal",
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
        GTK_DIALOG_MODAL,
        "_Créer", GTK_RESPONSE_OK,
        "_Annuler", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(content_area), vbox);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Nom du canal");
    gtk_box_append(GTK_BOX(vbox), entry);

    GtkWidget *check_button = gtk_check_button_new_with_label("Privé");
    gtk_box_append(GTK_BOX(vbox), check_button);

    g_object_set_data(G_OBJECT(dialog), "entry", entry);
    g_object_set_data(G_OBJECT(dialog), "check", check_button);

    g_signal_connect(dialog, "response", G_CALLBACK(on_create_channel_response), ctx);

    gtk_window_present(GTK_WINDOW(dialog));

}


static void on_add_member_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;

    GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "entry_user");
    GtkWidget *dropdown = g_object_get_data(G_OBJECT(dialog), "role_dropdown");

    if (response_id == GTK_RESPONSE_OK) {
        const char *username = gtk_editable_get_text(GTK_EDITABLE(entry));
        guint selected_index = gtk_drop_down_get_selected(GTK_DROP_DOWN(dropdown));

        const char *roles[] = { "member", "moderator", "admin" };
        const char *role = roles[selected_index];

        if (strlen(username) > 0) {
            snprintf(ctx->send_buffer, sizeof(ctx->send_buffer), "ADD_MEMBER:%s|%s|%s", ctx->selected_channel, username, role);
            send(ctx->ctx->client_fd, ctx->send_buffer, strlen(ctx->send_buffer), 0);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_add_member(GtkButton *button, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Ajouter un membre",
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
        GTK_DIALOG_MODAL,
        "_Ajouter", GTK_RESPONSE_OK,
        "_Annuler", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(content_area), vbox);

    GtkWidget *entry_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_user), "Nom d'utilisateur");
    gtk_box_append(GTK_BOX(vbox), entry_user);

    GtkStringList *role_list = gtk_string_list_new(NULL);
    gtk_string_list_append(role_list, "member");
    gtk_string_list_append(role_list, "moderator");
    gtk_string_list_append(role_list, "admin");
    GtkWidget *dropdown = gtk_drop_down_new(G_LIST_MODEL(role_list), NULL);
    gtk_box_append(GTK_BOX(vbox), dropdown);

    g_object_set_data(G_OBJECT(dialog), "entry_user", entry_user);
    g_object_set_data(G_OBJECT(dialog), "role_dropdown", dropdown);

    g_signal_connect(dialog, "response", G_CALLBACK(on_add_member_response), ctx);

    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_remove_member_response(GtkDialog *dialog, int response_id, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;

    GtkWidget *entry = g_object_get_data(G_OBJECT(dialog), "entry_user");

    if (response_id == GTK_RESPONSE_OK) {
        const char *username = gtk_editable_get_text(GTK_EDITABLE(entry));

        if (strlen(username) > 0) {
            snprintf(ctx->send_buffer, sizeof(ctx->send_buffer), "REMOVE_MEMBER:%s|%s", ctx->selected_channel, username);
            send(ctx->ctx->client_fd, ctx->send_buffer, strlen(ctx->send_buffer), 0);
        }
    }

    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void on_remove_member(GtkButton *button, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "Supprimer un membre",
        GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(button))),
        GTK_DIALOG_MODAL,
        "_Supprimer", GTK_RESPONSE_OK,
        "_Annuler", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(content_area), vbox);

    GtkWidget *entry_user = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_user), "Nom d'utilisateur");
    gtk_box_append(GTK_BOX(vbox), entry_user);

    g_object_set_data(G_OBJECT(dialog), "entry_user", entry_user);

    g_signal_connect(dialog, "response", G_CALLBACK(on_remove_member_response), ctx);

    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_channel_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    if (row != NULL) {
        ChatContext *ctx = (ChatContext *)user_data;
        const char *channel_name = g_object_get_data(G_OBJECT(row), "channel_name");
        if (!channel_name) return;


        char title[256];
        snprintf(title, sizeof(title), "Chat - %s", channel_name);
        gtk_label_set_text(GTK_LABEL(ctx->chat_label), title);

        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(ctx->chat_display));
        gtk_text_buffer_set_text(buffer, "", -1); 

        snprintf(ctx->selected_channel, sizeof(ctx->selected_channel), "%s", channel_name);

        char histo_cmd[512];
        snprintf(histo_cmd, sizeof(histo_cmd), "HIST:%s", channel_name);
        send(ctx->ctx->client_fd, histo_cmd, strlen(histo_cmd), 0);

    }
}

static void clear_list_box(GtkListBox *list_box) {
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(list_box));
    while (child != NULL) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(list_box, child);
        child = next;
    }
}

static void load_channels(ChatContext *ctx) {
 
    clear_list_box(GTK_LIST_BOX(ctx->channel_list));

    snprintf(ctx->send_buffer, sizeof(ctx->send_buffer), "CHAN:%s", ctx->ctx->username);
    send(ctx->ctx->client_fd, ctx->send_buffer, strlen(ctx->send_buffer), 0);


    char list_channel[1024];
    memset(list_channel, 0, sizeof(list_channel));

    int bytes_read = read(ctx->ctx->client_fd, list_channel, sizeof(list_channel) - 1);

    if (bytes_read <= 0) {
        printf("Erreur réception des canaux ou serveur déconnecté.\n");
        return; 
    }
    
    list_channel[bytes_read] = '\0';  
    printf("%s", list_channel);


    char *saveptr;
    char *token = strtok_r(list_channel + 5, "|", &saveptr); 

    while (token != NULL) {
        GtkWidget *label = gtk_label_new(token);
        GtkWidget *row = gtk_list_box_row_new();
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        g_object_set_data(G_OBJECT(row), "channel_name", g_strdup(token));
        gtk_list_box_append(GTK_LIST_BOX(ctx->channel_list), row);
        gtk_widget_show(row);
        token = strtok_r(NULL, "|", &saveptr);
    }    
}



static void load_users(ChatContext *ctx) {
    clear_list_box(GTK_LIST_BOX(ctx->connected_list));
    clear_list_box(GTK_LIST_BOX(ctx->disconnected_list));

    env_load("..", false);

    PGconn *conn = PQconnectdb("");
    if (PQstatus(conn) != CONNECTION_OK) {
        printf("Erreur connexion DB\n");
        PQfinish(conn);
        return;
    }

    PGresult *res_online = PQexec(conn, "SELECT username FROM users WHERE status = 'online'");
    int n_online = PQntuples(res_online);
    for (int i = 0; i < n_online; i++) {
        const char *username = PQgetvalue(res_online, i, 0);
        GtkWidget *label = gtk_label_new(username);
        gtk_list_box_append(GTK_LIST_BOX(ctx->connected_list), label);
    }
    PQclear(res_online);

    PGresult *res_offline = PQexec(conn, "SELECT username FROM users WHERE status = 'offline'");
    int n_offline = PQntuples(res_offline);
    for (int i = 0; i < n_offline; i++) {
        const char *username = PQgetvalue(res_offline, i, 0);
        GtkWidget *label = gtk_label_new(username);
        gtk_list_box_append(GTK_LIST_BOX(ctx->disconnected_list), label);
    }
    PQclear(res_offline);

    PQfinish(conn);
}

static void on_send_message(GtkButton *button, gpointer user_data) {
    ChatContext *ctx = (ChatContext *)user_data;
    const char *message = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));

    if (strlen(message) == 0) return;

    GtkListBoxRow *selected_row = gtk_list_box_get_selected_row(GTK_LIST_BOX(ctx->channel_list));
    if (!selected_row) return;

    const char *channel_name = g_object_get_data(G_OBJECT(selected_row), "channel_name");
    if (!channel_name) return;

    char buffer_message[2000];
    snprintf(buffer_message, sizeof(buffer_message), "MESS:%s|%s|%s", channel_name, ctx->ctx->username, message);
    send(ctx->ctx->client_fd, buffer_message, strlen(buffer_message), 0);

    gtk_editable_set_text(GTK_EDITABLE(ctx->entry), "");
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

    GtkWidget *btn_create_channel = gtk_button_new_with_label("Créer un canal");
    gtk_box_append(GTK_BOX(vbox_left), btn_create_channel);

    GtkWidget *chat_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_vexpand(chat_box, TRUE);
    gtk_box_append(GTK_BOX(main_hbox), chat_box);

    GtkWidget *chat_label = gtk_label_new("Chat - Aucun canal");
    gtk_box_append(GTK_BOX(chat_box), chat_label);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE); 
    gtk_widget_set_hexpand(scroll, TRUE); 
    gtk_box_append(GTK_BOX(chat_box), scroll);

    GtkWidget *chat_display = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_display), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_display), GTK_WRAP_WORD_CHAR);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), chat_display);

    GtkWidget *entry = gtk_entry_new();
    GtkWidget *btn_send = gtk_button_new_with_label("Envoyer");
    gtk_box_append(GTK_BOX(chat_box), entry);
    gtk_box_append(GTK_BOX(chat_box), btn_send);

    GtkWidget *vbox_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_size_request(vbox_right, 200, -1);
    gtk_box_append(GTK_BOX(main_hbox), vbox_right);

    GtkWidget *label_connected = gtk_label_new("Utilisateurs connectés");
    gtk_box_append(GTK_BOX(vbox_right), label_connected);
    GtkWidget *connected_list = gtk_list_box_new();
    gtk_box_append(GTK_BOX(vbox_right), connected_list);

    GtkWidget *label_disconnected = gtk_label_new("Utilisateurs déconnectés");
    gtk_box_append(GTK_BOX(vbox_right), label_disconnected);
    GtkWidget *disconnected_list = gtk_list_box_new();
    gtk_box_append(GTK_BOX(vbox_right), disconnected_list);

    GtkWidget *btn_add_member = gtk_button_new_with_label("Ajouter membre");
    gtk_box_append(GTK_BOX(vbox_right), btn_add_member);

    GtkWidget *btn_remove_member = gtk_button_new_with_label("Supprimer membre");
    gtk_box_append(GTK_BOX(vbox_right), btn_remove_member);

    ChatContext *ctx = g_malloc(sizeof(ChatContext));
    ctx->chat_label = chat_label;
    ctx->channel_list = channel_list;
    ctx->chat_display = chat_display;
    ctx->entry = entry;
    ctx->connected_list = connected_list;
    ctx->disconnected_list = disconnected_list;
    ctx->ctx = global_ctx;

    load_channels(ctx); 
    load_users(ctx);    

    GThread *listener_thread = g_thread_new("listener", listen_for_messages, ctx);

    g_signal_connect(channel_list, "row-selected", G_CALLBACK(on_channel_selected), ctx);
    g_signal_connect(btn_create_channel, "clicked", G_CALLBACK(on_create_channel), ctx);
    g_signal_connect(btn_send, "clicked", G_CALLBACK(on_send_message), ctx);
    g_signal_connect(btn_add_member, "clicked", G_CALLBACK(on_add_member), ctx);
    g_signal_connect(btn_remove_member, "clicked", G_CALLBACK(on_remove_member), ctx);

    

    g_signal_connect(window, "destroy", G_CALLBACK(g_free), ctx);

    gtk_window_present(GTK_WINDOW(window));
}
