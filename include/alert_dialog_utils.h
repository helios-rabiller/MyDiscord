#ifndef ALERT_DIALOG_UTILS_H
#define ALERT_DIALOG_UTILS_H

#include <gtk/gtk.h>

typedef struct {
    GtkWidget *parent;
    const char *message;
} AlertData;

void show_alert_dialog(GtkWidget *parent, const char *message);

#endif 
