#ifdef CHEETER_HAVE_X11

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "cheeter/backend.h"
#include "cheeter/log.h"

typedef struct {
  Display *dpy;
  Window root;
  Atom net_active_window;
  Atom net_wm_pid;
  Atom net_wm_name;
  Atom wm_class;

  CheeterHotkeyCallback hotkey_cb;
  void *hotkey_user_data;

  KeyCode hotkey_keycode;
  unsigned int hotkey_modifiers;
} X11Private;

static void x11_cleanup(CheeterBackend *self);

// Global pointer for the GDK filter callback
static CheeterBackend *g_x11_backend = NULL;

// Error handler to prevent crash on BadWindow etc.
static int x11_error_handler(Display *dpy, XErrorEvent *e) {
  char msg[128];
  XGetErrorText(dpy, e->error_code, msg, sizeof(msg));
  LOG_WARN("X11 Error: %s (Opcode: %d)", msg, e->request_code);
  return 0;
}

// ---- GDK Event Filter ----

static GdkFilterReturn x11_event_filter(GdkXEvent *xevent, GdkEvent *event,
                                        gpointer data) {
  (void)event;
  (void)data;

  if (!g_x11_backend || !g_x11_backend->priv)
    return GDK_FILTER_CONTINUE;

  X11Private *priv = (X11Private *)g_x11_backend->priv;
  XEvent *ev = (XEvent *)xevent;

  if (ev->type == KeyPress) {
    XKeyEvent *kev = &ev->xkey;
    // Mask out NumLock/CapsLock when comparing
    unsigned int clean_mods = kev->state & ~(LockMask | Mod2Mask);

    LOG_DEBUG("X11 KeyPress: keycode=%d, state=0x%x (clean=0x%x), expected: "
              "code=%d, mods=0x%x",
              kev->keycode, kev->state, clean_mods, priv->hotkey_keycode,
              priv->hotkey_modifiers);

    if (kev->keycode == priv->hotkey_keycode &&
        clean_mods == priv->hotkey_modifiers) {
      LOG_INFO("Hotkey triggered!");
      if (priv->hotkey_cb) {
        priv->hotkey_cb(priv->hotkey_user_data);
      }
      return GDK_FILTER_REMOVE; // Consume the event
    }
  }
  return GDK_FILTER_CONTINUE;
}

// ---- Hotkey Parsing Stub ----
// Ideally use something like libxkbcommon or bespoke logic.
// For MVP, simplistic parsing of "Super+Slash", "Ctrl+Alt+P"
static void parse_hotkey(Display *dpy, const char *hotkey_str, KeyCode *code,
                         unsigned int *mods) {
  // defaults
  *code = 0;
  *mods = 0;

  if (!hotkey_str)
    return;

  char **tokens = g_strsplit(hotkey_str, "+", -1);
  for (int i = 0; tokens[i]; i++) {
    char *t = tokens[i];
    if (g_ascii_strcasecmp(t, "Super") == 0)
      *mods |= Mod4Mask;
    else if (g_ascii_strcasecmp(t, "Control") == 0 ||
             g_ascii_strcasecmp(t, "Ctrl") == 0)
      *mods |= ControlMask;
    else if (g_ascii_strcasecmp(t, "Shift") == 0)
      *mods |= ShiftMask;
    else if (g_ascii_strcasecmp(t, "Alt") == 0)
      *mods |= Mod1Mask;
    else {
      KeySym sym = XStringToKeysym(t);
      if (sym != NoSymbol) {
        *code = XKeysymToKeycode(dpy, sym);
      }
    }
  }
  g_strfreev(tokens);
}

// ---- Backend Interface Implementation ----

static bool x11_init(CheeterBackend *self, const char *hotkey_str,
                     CheeterHotkeyCallback cb, void *user_data) {
  X11Private *priv = g_new0(X11Private, 1);
  self->priv = priv;

  // Use GDK's display instead of opening a separate one
  GdkDisplay *gdk_dpy = gdk_display_get_default();
  if (!gdk_dpy || !GDK_IS_X11_DISPLAY(gdk_dpy)) {
    LOG_ERROR("Could not get GDK X11 Display. Is X running?");
    x11_cleanup(self);
    return false;
  }

  priv->dpy = GDK_DISPLAY_XDISPLAY(gdk_dpy);
  if (!priv->dpy) {
    LOG_ERROR("Could not get X11 Display from GDK.");
    x11_cleanup(self);
    return false;
  }

  priv->root = DefaultRootWindow(priv->dpy);
  XSetErrorHandler(x11_error_handler);

  // Atoms
  priv->net_active_window = XInternAtom(priv->dpy, "_NET_ACTIVE_WINDOW", False);
  priv->net_wm_pid = XInternAtom(priv->dpy, "_NET_WM_PID", False);
  priv->net_wm_name = XInternAtom(priv->dpy, "_NET_WM_NAME", False);
  priv->wm_class = XInternAtom(priv->dpy, "WM_CLASS", False);

  // Setup Hotkey
  priv->hotkey_cb = cb;
  priv->hotkey_user_data = user_data;

  parse_hotkey(priv->dpy, hotkey_str, &priv->hotkey_keycode,
               &priv->hotkey_modifiers);

  if (priv->hotkey_keycode) {
    // Grab on root, with CapsLock/NumLock variants
    unsigned int ignored_masks[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (int i = 0; i < 4; i++) {
      XGrabKey(priv->dpy, priv->hotkey_keycode,
               priv->hotkey_modifiers | ignored_masks[i], priv->root, False,
               GrabModeAsync, GrabModeAsync);
    }
    XSync(priv->dpy, False);
    LOG_INFO("Grabbed X11 hotkey: %s (code: %d, mods: 0x%x)", hotkey_str,
             priv->hotkey_keycode, priv->hotkey_modifiers);
  } else {
    LOG_WARN("Could not parse hotkey: %s", hotkey_str);
  }

  // Install GDK event filter to receive X11 events through GTK's main loop
  g_x11_backend = self;
  gdk_window_add_filter(NULL, x11_event_filter, NULL);

  return true;
}

static AppIdentity *x11_get_active_app(CheeterBackend *self) {
  X11Private *priv = (X11Private *)self->priv;
  if (!priv || !priv->dpy)
    return NULL;

  AppIdentity *id = g_new0(AppIdentity, 1);

  // Get Active Window
  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char *prop = NULL;
  Window active_win = 0;

  if (XGetWindowProperty(priv->dpy, priv->root, priv->net_active_window, 0, 1,
                         False, XA_WINDOW, &actual_type, &actual_format,
                         &nitems, &bytes_after, &prop) == Success) {
    if (prop && nitems > 0) {
      active_win = *((Window *)prop);
    }
    if (prop)
      XFree(prop);
  }

  if (!active_win) {
    LOG_WARN("No active X11 window found.");
    return id; // empty
  }

  // Get WM_CLASS
  XClassHint class_hint;
  if (XGetClassHint(priv->dpy, active_win, &class_hint)) {
    if (class_hint.res_class)
      id->wm_class = g_strdup(class_hint.res_class);
    // Could also use res_name
    if (class_hint.res_name)
      XFree(class_hint.res_name);
    if (class_hint.res_class)
      XFree(class_hint.res_class);
  }

  // Get _NET_WM_PID
  pid_t pid = 0;
  if (XGetWindowProperty(priv->dpy, active_win, priv->net_wm_pid, 0, 1, False,
                         XA_CARDINAL, &actual_type, &actual_format, &nitems,
                         &bytes_after, &prop) == Success) {
    if (prop && nitems > 0) {
      pid = *((unsigned long *)prop);
    }
    if (prop)
      XFree(prop);
  }

  // Get Exe path from /proc
  if (pid > 0) {
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", pid);
    char exe_buf[1024];
    ssize_t len = readlink(proc_path, exe_buf, sizeof(exe_buf) - 1);
    if (len != -1) {
      exe_buf[len] = '\0';
      id->exe_path = g_strdup(exe_buf);
    }
  }

  // Get Window Title (_NET_WM_NAME or WM_NAME)
  // Simplified: try retrieving raw name
  // Correct way handles utf8 types, but for MVP XFetchName is fallback or
  // manual request Let's try XFetchName for simplicity, or _NET_WM_NAME
  char *name = NULL;
  if (XFetchName(priv->dpy, active_win, &name) > 0) {
    id->title = g_strdup(name);
    XFree(name);
  }

  return id;
}

static void x11_cleanup(CheeterBackend *self) {
  if (!self || !self->priv)
    return;

  // Remove event filter
  if (g_x11_backend == self) {
    gdk_window_remove_filter(NULL, x11_event_filter, NULL);
    g_x11_backend = NULL;
  }

  X11Private *priv = (X11Private *)self->priv;
  // Don't close the display - it's owned by GDK
  priv->dpy = NULL;
  g_free(priv);
  self->priv = NULL;
}

CheeterBackend *cheeter_backend_x11_new(void) {
  CheeterBackend *b = g_new0(CheeterBackend, 1);
  b->name = "x11";
  b->init = x11_init;
  b->get_active_app = x11_get_active_app;
  b->cleanup = x11_cleanup;
  return b;
}

#endif
