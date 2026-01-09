#ifndef CHEETER_PATHS_H
#define CHEETER_PATHS_H

// Override default directories (call before any get_*_dir calls)
void cheeter_set_config_dir_override(const char *path);
void cheeter_set_data_dir_override(const char *path);

char *cheeter_get_config_dir(void);
char *cheeter_get_data_dir(void);
char *cheeter_get_runtime_dir(void);
char *cheeter_get_socket_path(void);

#endif
