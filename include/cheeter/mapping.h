#ifndef CHEETER_MAPPING_H
#define CHEETER_MAPPING_H

#include <glib.h>

// Simple key-value store: app_key -> sheet_path
typedef struct {
  GHashTable *map; // char* (key) -> char* (value)
  char *file_path;
} MappingStore;

MappingStore *cheeter_mapping_load(const char *file_path);
void cheeter_mapping_save(MappingStore *store);
void cheeter_mapping_free(MappingStore *store);

const char *cheeter_mapping_get(MappingStore *store, const char *app_key);
void cheeter_mapping_set(MappingStore *store, const char *app_key,
                         const char *sheet_path);

#endif
