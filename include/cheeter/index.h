#ifndef CHEETER_INDEX_H
#define CHEETER_INDEX_H

#include <glib.h>

typedef struct {
  char *path;
  char *basename; // Lowercase, no extension
} SheetEntry;

typedef struct {
  GHashTable *sheets_by_basename; // char* -> SheetEntry*
  GList *all_sheets;              // List of SheetEntry*
} SheetIndex;

SheetIndex *cheeter_index_new(void);
void cheeter_index_free(SheetIndex *index);
void cheeter_index_scan_dir(SheetIndex *index, const char *dir_path);
const SheetEntry *cheeter_index_find_by_basename(SheetIndex *index,
                                                 const char *basename);

#endif
