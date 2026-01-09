#ifndef CHEETER_LOG_H
#define CHEETER_LOG_H

#include <stdarg.h>

typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR
} LogLevel;

void cheeter_log_init(int debug_enabled);
void cheeter_log(LogLevel level, const char *fmt, ...);

#define LOG_DEBUG(...) cheeter_log(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) cheeter_log(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...) cheeter_log(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(...) cheeter_log(LOG_LEVEL_ERROR, __VA_ARGS__)

#endif
