#include "cheeter/log.h"
#include "cheeter/mapping.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>

MappingStore *cheeter_mapping_load(const char *file_path) {
  MappingStore *store = g_new0(MappingStore, 1);
  store->file_path = g_strdup(file_path);
  store->map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

  if (!file_path)
    return store;

  // Load simple K=V or TSV lines
  FILE *f = fopen(file_path, "r");
  if (!f) {
    // Not an error, just new
    return store;
  }

  char line[1024];
  while (fgets(line, sizeof(line), f)) {
    // Strip newline
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
      line[len - 1] = '\0';

    // Simple parsing: first whitespace/cmp is separator
    // Or if TSV, strictly TAB. Let's support TAB or Space for leniency, or just
    // TAB as plan says. Plan says TSV.
    char *tab = strchr(line, '\t');
    if (tab) {
      *tab = '\0';
      char *key = g_strdup(line);
      char *val = g_strdup(tab + 1);
      // strip extra columns if we had them in older version
      char *next_tab = strchr(val, '\t');
      if (next_tab)
        *next_tab = '\0';

      g_hash_table_replace(store->map, key, val);
    }
  }
  fclose(f);
  return store;
}

void cheeter_mapping_save(MappingStore *store) {
  if (!store->file_path)
    return;

  FILE *f = fopen(store->file_path, "w");
  if (!f) {
    LOG_WARN("Could not write mappings to %s", store->file_path);
    return;
  }

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, store->map);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    fprintf(f, "%s\t%s\n", (char *)key, (char *)value);
  }
  fclose(f);
}

void cheeter_mapping_free(MappingStore *store) {
  if (!store)
    return;
  g_hash_table_destroy(store->map);
  g_free(store->file_path);
  g_free(store);
}

const char *cheeter_mapping_get(MappingStore *store, const char *app_key) {
  return (const char *)g_hash_table_lookup(store->map, app_key);
}

void cheeter_mapping_set(MappingStore *store, const char *app_key,
                         const char *sheet_path) {
  g_hash_table_replace(store->map, g_strdup(app_key), g_strdup(sheet_path));
  // Save on set
  cheeter_mapping_save(store);
}
