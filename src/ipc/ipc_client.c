#include "cheeter/ipc.h"
#include "cheeter/log.h"
#include <gio/gio.h>
#include <gio/gunixsocketaddress.h>
#include <glib.h>

bool cheeter_ipc_client_send(const char *socket_path, const char *message,
                             char **response_out) {
  GSocketClient *client = g_socket_client_new();
  GSocketAddress *addr = g_unix_socket_address_new(socket_path);
  GError *error = NULL;

  GSocketConnection *conn =
      g_socket_client_connect(client, G_SOCKET_CONNECTABLE(addr), NULL, &error);
  g_object_unref(addr);
  g_object_unref(client);

  if (!conn) {
    // Only log warning if not just a user query check
    // For CLI tools, returning false lets caller handle printing error
    if (error) {
      // It's expected to fail if daemon is not running.
      g_error_free(error);
    }
    return false;
  }

  GOutputStream *out = g_io_stream_get_output_stream(G_IO_STREAM(conn));

  // Send message + newline
  char *msg_nl = g_strdup_printf("%s\n", message);
  if (!g_output_stream_write_all(out, msg_nl, strlen(msg_nl), NULL, NULL,
                                 &error)) {
    g_free(msg_nl);
    g_error_free(error);
    g_object_unref(conn);
    return false;
  }
  g_free(msg_nl);

  // Read response
  GInputStream *in = g_io_stream_get_input_stream(G_IO_STREAM(conn));
  char buffer[1024];
  gssize n_read =
      g_input_stream_read(in, buffer, sizeof(buffer) - 1, NULL, &error);

  if (n_read >= 0) {
    buffer[n_read] = '\0';
    if (response_out) {
      *response_out = g_strdup(buffer);
    }
  } else {
    if (error)
      g_error_free(error);
  }

  g_object_unref(conn);
  return true;
}
