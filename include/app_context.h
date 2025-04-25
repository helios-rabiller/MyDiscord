#ifndef APP_CONTEXT_H
#define APP_CONTEXT_H

typedef struct {
    int client_fd;
    char username[64];
} AppContext;

#endif
