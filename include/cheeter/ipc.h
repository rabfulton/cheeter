#ifndef CHEETER_IPC_H
#define CHEETER_IPC_H

#include <stdbool.h>

// Server side
typedef void (*CheeterIpcCallback)(const char *command, void *user_data);

typedef struct CheeterIpcServer CheeterIpcServer;

CheeterIpcServer *cheeter_ipc_server_new(const char *socket_path,
                                         CheeterIpcCallback callback,
                                         void *user_data);
void cheeter_ipc_server_start(
    CheeterIpcServer *server); // Non-blocking if integrated with main loop, or
                               // blocking if simple
void cheeter_ipc_server_free(CheeterIpcServer *server);
// For now, let's assume we integrate with GMainLoop since we use glib heavily
void cheeter_ipc_server_attach_to_mainloop(CheeterIpcServer *server);

// Client side
bool cheeter_ipc_client_send(const char *socket_path, const char *message,
                             char **response_out);

#endif
