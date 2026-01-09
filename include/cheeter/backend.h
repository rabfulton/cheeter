#ifndef CHEETER_BACKEND_H
#define CHEETER_BACKEND_H

#include <glib.h>
#include <stdbool.h>

// Represents the discovered identity of the active application
typedef struct {
  char *desktop_id; // e.g., "org.gnome.Terminal"
  char *wm_class;   // e.g., "gnome-terminal-server"
  char *exe_path;   // e.g., "/usr/bin/gnome-terminal-server"
  char *title;      // e.g., "Terminal"
} AppIdentity;

void cheeter_app_identity_free(AppIdentity *id);

// Declarations for hotkey callback
typedef void (*CheeterHotkeyCallback)(void *user_data);

typedef struct CheeterBackend CheeterBackend;

struct CheeterBackend {
  const char *name; // "x11", "wayland", etc.

  // Virtual methods
  bool (*init)(CheeterBackend *self, const char *hotkey_str,
               CheeterHotkeyCallback cb, void *user_data);
  AppIdentity *(*get_active_app)(CheeterBackend *self);
  void (*cleanup)(CheeterBackend *self);

  // Internal state
  void *priv;
};

// Factory methods
CheeterBackend *cheeter_backend_x11_new(void);
// CheeterBackend *cheeter_backend_wayland_new(void);

#endif
