#include "xdwlw-error.h"

static enum xdwlw_errors xdwlw_errcode = 0;
static char xdwlw_errmsg[1024];

void xdwlw_error_set(enum xdwlw_errors errcode, const char *errmsg, ...) {
  xdwlw_errcode = errcode;
  va_list args;
  va_start(args, errmsg);
  vsnprintf(xdwlw_errmsg, sizeof(xdwlw_errmsg), errmsg, args);
  va_end(args);
}

void xdwlw_error_print() {
  if (xdwlw_errcode >= 0) {
    fprintf(stderr, "xdwlw: %s\n", xdwlw_errmsg);
    xdwlw_errcode = 0;
  }
}

inline char *xdwlw_error_get_msg() {
  return xdwlw_errmsg[0] != '\0' ? xdwlw_errmsg : NULL;
}
inline enum xdwlw_errors xdwlw_error_get_code() { return xdwlw_errcode; }
