#include "xdwayland-client.h"

#define xdwlw_error_add_msg(m)                                                 \
  do {                                                                         \
    if (errmsg_fmt != NULL) {                                                  \
      va_start(args, errmsg_fmt);                                              \
      vsnprintf(xdwlw_err_msg, sizeof(xdwlw_err_msg), errmsg_fmt, args);       \
      va_end(args);                                                            \
    } else {                                                                   \
      snprintf(xdwlw_err_msg, sizeof(xdwlw_err_msg), (m));                     \
    }                                                                          \
  } while (0)

static enum xdwl_errors xdwl_errcode = 0;
static char xdwl_errmsg[1024];

void xdwl_error_set(enum xdwl_errors errcode, const char *errmsg, ...) {
  xdwl_errcode = errcode;
  va_list args;
  va_start(args, errmsg);
  vsnprintf(xdwl_errmsg, sizeof(xdwl_errmsg), errmsg, args);
  va_end(args);
}

void xdwl_error_print() {
  if (xdwl_errcode >= 0) {
    fprintf(stderr, "%s\n", xdwl_errmsg);
    xdwl_errcode = 0;
  }
}

void xdwl_raise(xdwl_proxy *proxy) {
  xdwl_error_print();
  if (proxy != NULL) {
    xdwl_proxy_destroy(proxy);
  }
}

inline char *xdwl_error_get_msg() {
  return xdwl_errmsg[0] != '\0' ? xdwl_errmsg : NULL;
}
inline enum xdwl_errors xdwl_error_get_code() { return xdwl_errcode; }
