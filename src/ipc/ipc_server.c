#include "cheeter/ipc.h"
#include "cheeter/log.h"
#include <gio/gio.h>
#include <glib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

struct CheeterIpcServer {
  char *socket_path;
  CheeterIpcCallback callback;
  void *user_data;
  GSocketService *service;
};

static gboolean on_incoming_connection(GSocketService *service,
                                       GSocketConnection *connection,
                                       GObject *source_object,
                                       gpointer user_data) {
  CheeterIpcServer *server = (CheeterIpcServer *)user_data;

  // Simple line-based or size-prefixed protocol.
  // For MVP CLI, just read all until close or use GDataInputStream.

  GInputStream *input = g_io_stream_get_input_stream(G_IO_STREAM(connection));
  GDataInputStream *data_in = g_data_input_stream_new(input);
  GError *error = NULL;
  char *line;
  gsize len;

  // Read a single line command
  line = g_data_input_stream_read_line(data_in, &len, NULL, &error);

  if (line) {
    LOG_DEBUG("IPC Received: %s", line);
    if (server->callback) {
      server->callback(line, server->user_data);
    }

    // Simple response
    GOutputStream *output =
        g_io_stream_get_output_stream(G_IO_STREAM(connection));
    g_output_stream_write_all(output, "OK\n", 3, NULL, NULL, NULL);

    g_free(line);
  } else if (error) {
    LOG_ERROR("IPC read error: %s", error->message);
    g_error_free(error);
  }

  g_object_unref(data_in);
  return FALSE; // Don't keep the connection object alive, strictly one-shot for
                // now (or handle differently)
}

CheeterIpcServer *cheeter_ipc_server_new(const char *socket_path,
                                         CheeterIpcCallback callback,
                                         void *user_data) {
  CheeterIpcServer *server = g_new0(CheeterIpcServer, 1);
  server->socket_path = g_strdup(socket_path);
  server->callback = callback;
  server->user_data = user_data;
  return server;
}

void cheeter_ipc_server_attach_to_mainloop(CheeterIpcServer *server) {
  server->service = g_socket_service_new();
  GError *error = NULL;

  GSocketAddress *addr = g_unix_socket_address_new(server->socket_path);

  // Unlink old socket if exists
  unlink(server->socket_path);

  if (!g_socket_listener_add_address(
          G_SOCKET_LISTENER(server->service), addr, G_SOCKET_TYPE_STREAM,
          G_SOCKET_PROTOCOL_DEFAULT, NULL, NULL, &error)) {
    LOG_ERROR("Failed to bind socket %s: %s", server->socket_path,
              error->message);
    g_error_free(error);
    g_object_unref(addr);
    return;
  }

  g_object_unref(addr);

  g_signal_connect(server->service, "incoming",
                   G_CALLBACK(on_incoming_connection), server);
  g_socket_service_start(server->service);
  LOG_INFO("IPC Server listening on %s", server->socket_path);
}

void cheeter_ipc_server_free(CheeterIpcServer *server) {
  if (!server)
    return;
  if (server->service) {
    g_socket_service_stop(server->service);
    g_object_unref(server->service);
  }
  g_free(server->socket_path);
  g_free(server);
}

void cheeter_ipc_server_start(CheeterIpcServer *server) {
  // No-op if using attach_to_mainloop + g_main_loop_run in main
  (void)server;
}
