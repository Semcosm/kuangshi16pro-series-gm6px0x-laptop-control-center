#include "common/lcc_log.h"

#include <stdarg.h>
#include <stdio.h>

static void lcc_log_vprint(const char *level, const char *fmt, va_list args) {
  (void)fprintf(stderr, "[%s] ", level);
  (void)vfprintf(stderr, fmt, args);
  (void)fputc('\n', stderr);
}

void lcc_log_info(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  lcc_log_vprint("info", fmt, args);
  va_end(args);
}

void lcc_log_warn(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  lcc_log_vprint("warn", fmt, args);
  va_end(args);
}

void lcc_log_error(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  lcc_log_vprint("error", fmt, args);
  va_end(args);
}
