#include "cheeter/log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static int g_debug = 0;

void cheeter_log_init(int debug_enabled) { g_debug = debug_enabled; }

void cheeter_log(LogLevel level, const char *fmt, ...) {
  if (level == LOG_LEVEL_DEBUG && !g_debug) {
    return;
  }

  const char *level_str = "INFO";
  FILE *out = stdout;

  switch (level) {
  case LOG_LEVEL_DEBUG:
    level_str = "DEBUG";
    break;
  case LOG_LEVEL_INFO:
    level_str = "INFO";
    break;
  case LOG_LEVEL_WARN:
    level_str = "WARN";
    out = stderr;
    break;
  case LOG_LEVEL_ERROR:
    level_str = "ERROR";
    out = stderr;
    break;
  }

  fprintf(out, "[%s] ", level_str);

  va_list args;
  va_start(args, fmt);
  vfprintf(out, fmt, args);
  va_end(args);

  fprintf(out, "\n");
}
