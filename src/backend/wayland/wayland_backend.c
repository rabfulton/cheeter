#ifdef CHEETER_HAVE_ATSPI
// This whole backend is conditional on AT-SPI support usually,
// as pure Wayland protocols don't expose 'active window' trivially.

#include "cheeter/backend.h"
#include "cheeter/log.h"
#include <glib.h>
#include <stdio.h>

// Forward declaration of factory function
CheeterBackend *cheeter_backend_wayland_new(void);

typedef struct {
  // DBus connection for AT-SPI
  // GDBusConnection *bus;

  // AT-SPI specific state
  // AtspiDeviceListener *listener;

  // Layer Shell state
  // struct zwlr_layer_shell_v1 *layer_shell;
} WaylandPrivate;

static bool wayland_init(CheeterBackend *self, const char *hotkey_str,
                         CheeterHotkeyCallback cb, void *user_data) {
  (void)hotkey_str;
  (void)cb;
  (void)user_data;

  WaylandPrivate *priv = g_new0(WaylandPrivate, 1);
  self->priv = priv;

  LOG_WARN("Wayland backend initialized (MOCK). Real implementation would:");
  LOG_WARN("  1. Connect to AT-SPI registry to listen for 'window:activate' "
           "events.");
  LOG_WARN("  2. Use XDG Desktop Portal (GlobalShortcuts) or Compositor "
           "shortcuts for hotkeys.");
  LOG_WARN("  3. Use wlr-layer-shell for overlay window positioning.");

  return true;
}

static AppIdentity *wayland_get_active_app(CheeterBackend *self) {
  (void)self;
  LOG_INFO("Wayland: get_active_app called (MOCK).");

  /*
     Implementation logic:
     1. Query AT-SPI for current focused object.
     2. Walk up tree to find 'Application' role.
     3. Retrieve 'Toolkit', 'Name', and try to map to .desktop file.
  */

  // Return dummy for testing if this backend were active
  // return NULL;
  return NULL;
}

static void wayland_cleanup(CheeterBackend *self) {
  if (!self->priv)
    return;
  g_free(self->priv);
  self->priv = NULL;
}

CheeterBackend *cheeter_backend_wayland_new(void) {
  CheeterBackend *b = g_new0(CheeterBackend, 1);
  b->name = "wayland";
  b->init = wayland_init;
  b->get_active_app = wayland_get_active_app;
  b->cleanup = wayland_cleanup;
  return b;
}

#endif
