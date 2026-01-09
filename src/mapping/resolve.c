#include "cheeter/index.h"
#include "cheeter/log.h"
#include "cheeter/mapping.h"
#include <glib.h>
#include <string.h>

// For now, app identity is just a string (app_key).
// In Phase 2, we will have a struct AppIdentity.
// But the resolver interface can take the calculated key for now.

const char *cheeter_resolve_sheet(SheetIndex *index, MappingStore *store,
                                  const char *app_key) {
  if (!app_key)
    return NULL;

  LOG_DEBUG("Resolving sheet for key: %s", app_key);

  // 1. Check mapping store (exact match)
  const char *mapped = cheeter_mapping_get(store, app_key);
  if (mapped) {
    LOG_INFO("Found explicit mapping for %s -> %s", app_key, mapped);
    // Verify it exists? If not, maybe fall through?
    // For MVP, assume if mapped, return it. If file missing, UI will handle.
    return mapped;
  }

  // 2. Heuristic match
  // app_key might be "desktop:org.gnome.Terminal" or "exe:bash"
  // Extract the "name" part
  const char *name_part = strchr(app_key, ':');
  if (name_part) {
    name_part++; // skip ':'
  } else {
    name_part = app_key;
  }

  // Normalized lookup in index
  char *normalized = g_ascii_strdown(name_part, -1);
  const SheetEntry *entry = cheeter_index_find_by_basename(index, normalized);
  g_free(normalized);

  if (entry) {
    LOG_INFO("Found heuristic match for %s -> %s", app_key, entry->path);
    // We could auto-save this mapping, but maybe safer to let user confirm.
    // For MVP "fast workflow", let's just return it without saving.
    return entry->path;
  }

  return NULL;
}
