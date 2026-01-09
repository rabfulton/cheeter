#include "cheeter/config.h"
#include "cheeter/ipc.h"
#include "cheeter/log.h"
#include "cheeter/paths.h"
#include <glib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "cheeter/backend.h"
#include "cheeter/index.h"
#include "cheeter/mapping.h"
#include "cheeter/ui.h"

// Forward factory decls
CheeterBackend *cheeter_backend_x11_new(void);
CheeterBackend *cheeter_backend_wayland_new(void);

// Forward decl
const char *cheeter_resolve_sheet(SheetIndex *index, MappingStore *store,
                                  const char *app_key);

// Internal logic to handle toggle request (simulated or real)
void handle_toggle(void);

// CLI options
static gchar *opt_config_dir = NULL;
static gchar *opt_hotkey = NULL;

static GOptionEntry cli_entries[] = {
    {"config-dir", 'c', 0, G_OPTION_ARG_FILENAME, &opt_config_dir,
     "Use DIR as config directory", "DIR"},
    {"hotkey", 'k', 0, G_OPTION_ARG_STRING, &opt_hotkey,
     "Override hotkey (e.g. 'Super+slash', 'Ctrl+Alt+h')", "KEY"},
    {NULL}};

// static GMainLoop *mainloop = NULL; // Removed, using GTK loop
static SheetIndex *g_index = NULL;
static MappingStore *g_store = NULL;
static CheeterBackend *g_backend = NULL;

static void on_ipc_command(const char *command, void *user_data) {
  (void)user_data;
  if (g_str_has_prefix(command, "TOGGLE")) {
    LOG_INFO("IPC: TOGGLE request");
    handle_toggle();
  } else if (g_str_has_prefix(command, "STATUS")) {
    LOG_INFO("Executing STATUS action...");
    // Re-scan for verification
    // cheeter_index_scan_dir(g_index, ...);
  } else if (g_str_has_prefix(command, "QUIT")) {
    LOG_INFO("Quitting daemon...");
    cheeter_ui_quit();
  }
}

static void handle_sigterm(int signum) {
  (void)signum;
  // gtk_main_quit() is safe enough to call from here usually,
  // or set a flag, but for simple daemon this is ok-ish.
  LOG_INFO("Caught SIGTERM, exiting...");
  cheeter_ui_quit();
}

int main(int argc, char *argv[]) {
  // Basic signal handling
  struct sigaction sa;
  sa.sa_handler = handle_sigterm;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);

  // Parse CLI options (integrates with GTK's argc/argv processing)
  GOptionContext *ctx = g_option_context_new("- cheat sheet overlay daemon");
  g_option_context_add_main_entries(ctx, cli_entries, NULL);
  g_option_context_add_group(ctx, gtk_get_option_group(TRUE));

  GError *error = NULL;
  if (!g_option_context_parse(ctx, &argc, &argv, &error)) {
    g_printerr("Option parsing failed: %s\\n", error->message);
    g_error_free(error);
    g_option_context_free(ctx);
    return 1;
  }
  g_option_context_free(ctx);

  // Apply config-dir override BEFORE loading config
  if (opt_config_dir) {
    cheeter_set_config_dir_override(opt_config_dir);
    // Also use as data dir if not otherwise specified
    cheeter_set_data_dir_override(opt_config_dir);
  }

  // Initialize UI (GTK) - must be after option parsing
  cheeter_ui_init(&argc, &argv);

  // Initialize logging
  cheeter_log_init(1); // Default to debug for now

  // Check if another instance is already running
  char *socket_path = cheeter_get_socket_path();
  if (g_file_test(socket_path, G_FILE_TEST_EXISTS)) {
    // Try to connect - if successful, another instance is running
    GSocketClient *client = g_socket_client_new();
    GSocketAddress *addr = g_unix_socket_address_new(socket_path);
    GSocketConnection *conn =
        g_socket_client_connect(client, G_SOCKET_CONNECTABLE(addr), NULL, NULL);

    if (conn) {
      g_object_unref(conn);
      g_object_unref(addr);
      g_object_unref(client);
      g_printerr("cheeterd is already running (socket: %s)\n", socket_path);
      g_free(socket_path);
      return 1;
    }

    // Socket exists but not connectable - stale socket, remove it
    LOG_WARN("Removing stale socket: %s", socket_path);
    unlink(socket_path);
    g_object_unref(addr);
    g_object_unref(client);
  }
  g_free(socket_path);

  LOG_INFO("Starting cheeterd...");

  // Load Config
  char *config_dir = cheeter_get_config_dir();
  char *config_path = g_build_filename(config_dir, "config.ini", NULL);
  CheeterConfig *config = cheeter_config_load(config_path);
  cheeter_log_init(config->debug_log);

  // Set zoom level
  cheeter_ui_set_zoom_level(config->zoom_level);

  // Override hotkey from CLI if specified
  if (opt_hotkey) {
    g_free(config->hotkey);
    config->hotkey = g_strdup(opt_hotkey);
    LOG_INFO("Hotkey (CLI override): %s", config->hotkey);
  } else {
    LOG_INFO("Hotkey: %s", config->hotkey);
  }

  // Initialize Index & Store
  g_index = cheeter_index_new();
  char *sheet_dir;
  if (config->sheets_dir) {
    sheet_dir = g_strdup(config->sheets_dir);
    LOG_INFO("Sheets directory (config): %s", sheet_dir);
  } else {
    char *data_dir = cheeter_get_data_dir();
    sheet_dir = g_build_filename(data_dir, "sheets", NULL);
    g_free(data_dir);
  }
  g_mkdir_with_parents(sheet_dir, 0755);

  cheeter_index_scan_dir(g_index, sheet_dir);

  // Mappings
  char *map_file =
      g_build_filename(cheeter_get_config_dir(), "mappings.tsv", NULL);
  g_store = cheeter_mapping_load(map_file);

  g_free(sheet_dir);
  g_free(map_file);

  // Setup IPC
  char *ipc_socket_path = cheeter_get_socket_path();
  CheeterIpcServer *ipc =
      cheeter_ipc_server_new(ipc_socket_path, on_ipc_command, NULL);
  cheeter_ipc_server_attach_to_mainloop(ipc);

  // Backend Init
  // TODO: better selection logic (check WAYLAND_DISPLAY etc)
#ifdef CHEETER_HAVE_X11
  if (!g_backend) {
    // Priority: X11 for now in this env
    LOG_INFO("Initializing X11 Backend...");
    g_backend = cheeter_backend_x11_new();
  }
#endif

#ifdef CHEETER_HAVE_ATSPI
  // If X11 failed or not present, try Wayland (Mock)
  if (!g_backend && g_getenv("WAYLAND_DISPLAY")) {
    LOG_INFO("Initializing Wayland Backend (Mock)...");
    g_backend = cheeter_backend_wayland_new();
  }
#endif

  if (g_backend) {
    if (!g_backend->init(g_backend, config->hotkey,
                         (CheeterHotkeyCallback)handle_toggle, NULL)) {
      LOG_ERROR("Backend failed to initialize.");
      // Non-fatal? Or exit? Let's keep running for IPC.
    }
  } else {
    LOG_WARN("No backend available (or X11 not compiled). Hotkeys/ActiveApp "
             "won't work.");
  }

  // Main Loop
  LOG_INFO("Entering UI/Main Loop...");
  cheeter_ui_run();

  // Cleanup
  cheeter_ipc_server_free(ipc);
  if (g_backend) {
    g_backend->cleanup(g_backend);
    g_free(g_backend);
  }
  if (g_index)
    cheeter_index_free(g_index);
  if (g_store)
    cheeter_mapping_free(g_store);
  cheeter_config_free(config);
  g_free(config_path);
  g_free(config_dir);
  g_free(ipc_socket_path);
  // if (mainloop) g_main_loop_unref(mainloop);

  LOG_INFO("cheeterd exited gracefully.");
  return 0;
}

void handle_toggle(void) {
  LOG_INFO("Action: Toggle/Show Cheatsheet");

  AppIdentity *id = NULL;
  char *app_key = NULL;

  if (g_backend && g_backend->get_active_app) {
    id = g_backend->get_active_app(g_backend);
  }

  if (id) {
    LOG_INFO("Active App: Title='%s', WM_CLASS='%s', Exe='%s'", id->title,
             id->wm_class, id->exe_path);

    // Build a primary key for resolution.
    // Priority: Exe > WM_CLASS > Title (simple MVP)
    // Ideally we check all candidates.
    if (id->exe_path) {
      char *base = g_path_get_basename(id->exe_path);
      app_key = g_strdup_printf("exe:%s", base);
      g_free(base);
    } else if (id->wm_class) {
      app_key = g_strdup_printf("class:%s", id->wm_class);
    } else {
      app_key = g_strdup("unknown");
    }
  } else {
    LOG_WARN("Could not determine active app.");
    app_key = g_strdup("unknown");
  }

  const char *sheet = cheeter_resolve_sheet(g_index, g_store, app_key);
  if (sheet) {
    LOG_INFO(">>> SHOW SHEET: %s <<<", sheet);
  } else {
    LOG_INFO(">>> NO SHEET FOUND for %s <<<", app_key);
  }

  // Toggle UI
  cheeter_ui_toggle(sheet);

  if (id)
    cheeter_app_identity_free(id);
  g_free(app_key);
}
