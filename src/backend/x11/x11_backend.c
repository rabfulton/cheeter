#ifdef CHEETER_HAVE_X11

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
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

// Helper to check if a process has WINDOWID environment variable matching
// target
static gboolean match_window_id(pid_t pid, Window target_win) {
  if (target_win == 0)
    return FALSE;

  char path[64];
  snprintf(path, sizeof(path), "/proc/%d/environ", pid);
  FILE *f = fopen(path, "rb");
  if (!f)
    return FALSE;

  gboolean found = FALSE;
  char buf[4096];
  size_t bytes = fread(buf, 1, sizeof(buf), f);
  fclose(f);

  if (bytes > 0) {
    char needle[64];
    snprintf(needle, sizeof(needle), "WINDOWID=%lu", target_win);

    // Environ entries are separated by \0.
    // We can scan the buffer.
    size_t pos = 0;
    while (pos < bytes) {
      if (strncmp(buf + pos, needle, strlen(needle)) == 0) {
        // Ensure partial match safety (e.g. WINDOWID=123 vs WINDOWID=1234)
        // The next char must be \0 or end of buffer if strictly matching
        // "VAR=VAL" but we constructed needle as VAR=VAL. Wait, snprintf
        // creates "WINDOWID=12345". In environ: "WINDOWID=12345\0" So we check
        // if buf[pos+strlen(needle)] == '\0'
        if (pos + strlen(needle) >= bytes ||
            buf[pos + strlen(needle)] == '\0') {
          found = TRUE;
        }
        break;
      }
      // Advance to next \0
      while (pos < bytes && buf[pos] != '\0')
        pos++;
      pos++; // Skip \0
    }
  }
  return found;
}

// Helper to find the deepest child process of a given PID
static pid_t get_deepest_child(pid_t start_pid, Window match_window) {
  pid_t current_pid = start_pid;
  gboolean branch_locked =
      FALSE; // Set to TRUE once we find a child claiming the window

  while (1) {
    DIR *proc = opendir("/proc");
    if (!proc) {
      LOG_WARN("Could not open /proc (errno: %d)", errno);
      break;
    }

    struct dirent *ent;
    pid_t found_child = 0;

    // We iterate all children.
    // If not locked yet, pick the one matching match_window.
    // If locked (or match_window is 0), pick any (or newest).

    // To handle "multiple children", we'd ideally scan all then decide.
    // Optimization: First pass check for window_id match?
    // Or just check each as we go?

    while ((ent = readdir(proc))) {
      if (!isdigit(*ent->d_name))
        continue;
      pid_t pid = atoi(ent->d_name);
      if (pid == current_pid || pid == getpid())
        continue;

      char path[64];
      snprintf(path, sizeof(path), "/proc/%d/stat", pid);
      FILE *f = fopen(path, "r");
      if (f) {
        char buf[1024];
        if (fgets(buf, sizeof(buf), f)) {
          char *rparen = strrchr(buf, ')');
          if (rparen) {
            char state;
            pid_t ppid;
            if (sscanf(rparen + 2, "%c %d", &state, &ppid) == 2) {
              if (ppid == current_pid) {
                // It is a child.
                // If we haven't locked onto a branch yet, and we have a target
                // window:
                if (!branch_locked && match_window != 0) {
                  if (match_window_id(pid, match_window)) {
                    LOG_DEBUG("Child %d matches WINDOWID %lu", pid,
                              match_window);
                    found_child = pid;
                    branch_locked = TRUE; // Descend only this branch now
                    // Don't need to scan others for this level?
                    // Assuming unique WINDOWID per process tree usually.
                    break; // Stop readdir, take this one
                  }
                }

                // If we didn't match (or don't care), populate found_child if
                // empty But if we later find a match, we prefer that. So
                // current logic of 'break' on first found is bad if we are
                // looking for a specific one. We should scan ALL children at
                // this level.
                if (!found_child)
                  found_child = pid;
              }
            }
          }
        }
        fclose(f);
      }
      if (branch_locked && found_child)
        break;
    }
    closedir(proc);

    if (found_child) {
      // LOG_DEBUG("Descend: %d -> %d", current_pid, found_child);
      current_pid = found_child;
    } else {
      break; // No more children
    }
  }
  return (current_pid == start_pid) ? 0 : current_pid;
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
    // Check for child process (e.g. vim inside xterm)
    // Pass active_win to ensure we follow the process that owns this window (if
    // defined)
    pid_t child_pid = get_deepest_child(pid, active_win);
    if (child_pid > 0) {
      LOG_DEBUG("Window PID %d resolved to child PID %d", pid, child_pid);
      pid = child_pid;
    }

    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/exe", pid);
    char exe_buf[1024];
    ssize_t len = readlink(proc_path, exe_buf, sizeof(exe_buf) - 1);
    if (len != -1) {
      exe_buf[len] = '\0';
      id->exe_path = g_strdup(exe_buf);
      LOG_DEBUG("Resolved Exe: %s", id->exe_path);
    } else {
      LOG_WARN("readlink failed for %s", proc_path);
    }
  } else {
    LOG_WARN("No PID found for window");
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
