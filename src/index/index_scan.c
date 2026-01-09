#include "cheeter/index.h"
#include "cheeter/log.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>

static void sheet_entry_free(gpointer data) {
  SheetEntry *entry = (SheetEntry *)data;
  if (entry) {
    g_free(entry->path);
    g_free(entry->basename);
    g_free(entry);
  }
}

SheetIndex *cheeter_index_new(void) {
  SheetIndex *index = g_new0(SheetIndex, 1);
  index->sheets_by_basename =
      g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);
  // Note: We manage memory of SheetEntries in the all_sheets list (or just one
  // of them), but the hash table keys/values point to the same data. For
  // simplicity, let's treat `all_sheets` as the owner for freeing, or do a
  // custom free.
  return index;
}

void cheeter_index_free(SheetIndex *index) {
  if (!index)
    return;
  g_hash_table_destroy(index->sheets_by_basename);
  g_list_free_full(index->all_sheets, sheet_entry_free);
  g_free(index);
}

void cheeter_index_scan_dir(SheetIndex *index, const char *dir_path) {
  GDir *dir = g_dir_open(dir_path, 0, NULL);
  if (!dir) {
    LOG_WARN("Could not open sheet directory: %s", dir_path);
    return;
  }

  const char *filename;
  int count = 0;
  while ((filename = g_dir_read_name(dir))) {
    if (g_str_has_suffix(filename, ".pdf")) {
      SheetEntry *entry = g_new0(SheetEntry, 1);
      entry->path = g_build_filename(dir_path, filename, NULL);

      // Basename = lowercase, no extension
      char *no_ext = g_strndup(filename, strlen(filename) - 4);
      entry->basename = g_ascii_strdown(no_ext, -1);
      g_free(no_ext);

      index->all_sheets = g_list_prepend(index->all_sheets, entry);
      g_hash_table_insert(index->sheets_by_basename, entry->basename, entry);

      LOG_DEBUG("Indexed sheet: %s -> %s", entry->basename, entry->path);
      count++;
    }
  }
  g_dir_close(dir);
  LOG_INFO("Indexed %d sheets in %s", count, dir_path);
}

const SheetEntry *cheeter_index_find_by_basename(SheetIndex *index,
                                                 const char *basename) {
  return (const SheetEntry *)g_hash_table_lookup(index->sheets_by_basename,
                                                 basename);
}
