#ifndef APP_CHAT_H
#define APP_CHAT_H

#include <gtk/gtk.h>
#include "app_context.h"

void show_chat_window(GtkApplication *app, GtkWindow *previous_window, AppContext *global_ctx);

#endif
