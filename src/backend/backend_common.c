#include "cheeter/backend.h"
#include <glib.h>

void cheeter_app_identity_free(AppIdentity *id) {
  if (!id)
    return;
  g_free(id->desktop_id);
  g_free(id->wm_class);
  g_free(id->exe_path);
  g_free(id->title);
  g_free(id);
}
