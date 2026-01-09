#include "cheeter/ipc.h"
#include "cheeter/log.h"
#include "cheeter/paths.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
  printf("Usage: %s <command>\n", prog);
  printf("Commands:\n");
  printf("  toggle       Toggle the cheatsheet overlay\n");
  printf("  search       Open search UI\n");
  printf("  status       Check daemon status\n");
  printf("  quit         Stop the daemon\n");
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage(argv[0]);
    return 1;
  }

  const char *cmd = argv[1];
  char *ipc_cmd = NULL;

  if (strcmp(cmd, "toggle") == 0) {
    ipc_cmd = g_strdup("TOGGLE");
  } else if (strcmp(cmd, "search") == 0) {
    ipc_cmd = g_strdup("SEARCH");
  } else if (strcmp(cmd, "status") == 0) {
    ipc_cmd = g_strdup("STATUS");
  } else if (strcmp(cmd, "quit") == 0) {
    ipc_cmd = g_strdup("QUIT");
  } else {
    print_usage(argv[0]);
    return 1;
  }

  char *socket_path = cheeter_get_socket_path();
  char *response = NULL;

  if (!cheeter_ipc_client_send(socket_path, ipc_cmd, &response)) {
    fprintf(stderr,
            "Error: Could not connect to cheeterd at %s. Is it running?\n",
            socket_path);
    g_free(socket_path);
    g_free(ipc_cmd);
    return 1;
  }

  if (response) {
    printf("%s", response); // Response includes newline usually
    g_free(response);
  }

  g_free(socket_path);
  g_free(ipc_cmd);
  return 0;
}
