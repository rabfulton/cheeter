#ifndef CHEETER_CONFIG_H
#define CHEETER_CONFIG_H

#include <stdbool.h>

typedef struct {
  char *hotkey;
  char *sheets_dir; // Custom path to cheat sheets directory (NULL = default)
  double zoom_level;
  bool debug_log;
} CheeterConfig;

CheeterConfig *cheeter_config_load(const char *path);
void cheeter_config_free(CheeterConfig *config);

#endif
