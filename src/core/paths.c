#include "cheeter/paths.h"
#include <glib.h>
#include <sys/stat.h>

// Override paths (set via CLI)
static char *g_config_dir_override = NULL;
static char *g_data_dir_override = NULL;

void cheeter_set_config_dir_override(const char *path) {
  g_free(g_config_dir_override);
  g_config_dir_override = path ? g_strdup(path) : NULL;
}

void cheeter_set_data_dir_override(const char *path) {
  g_free(g_data_dir_override);
  g_data_dir_override = path ? g_strdup(path) : NULL;
}

static char *ensure_dir(const char *base, const char *subdir) {
  char *path = g_build_filename(base, subdir, NULL);
  g_mkdir_with_parents(path, 0700);
  return path;
}

static char *ensure_dir_simple(const char *path) {
  g_mkdir_with_parents(path, 0700);
  return g_strdup(path);
}

char *cheeter_get_config_dir(void) {
  if (g_config_dir_override)
    return ensure_dir_simple(g_config_dir_override);
  return ensure_dir(g_get_user_config_dir(), "cheeter");
}

char *cheeter_get_data_dir(void) {
  if (g_data_dir_override)
    return ensure_dir_simple(g_data_dir_override);
  return ensure_dir(g_get_user_data_dir(), "cheeter");
}

char *cheeter_get_runtime_dir(void) {
  // XDG_RUNTIME_DIR is best for sockets
  const char *runtime = g_get_user_runtime_dir();
  return ensure_dir(runtime, "cheeter");
}

char *cheeter_get_socket_path(void) {
  char *dir = cheeter_get_runtime_dir();
  char *sock = g_build_filename(dir, "cheeter.sock", NULL);
  g_free(dir);
  return sock;
}
