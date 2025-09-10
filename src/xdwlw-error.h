#ifndef XDWLW_ERROR_H
#define XDWLW_ERROR_H

#include "xdwlw-common.h"
#include "xdwlw-types.h"

enum xdwlw_errors {
  XDWLWE_DCONN = 1,
  XDWLWE_DSTART,
  XDWLWE_DCLOSE,
  XDWLWE_DRECV,
  XDWLWE_DSEND,
  XDWLWE_LONGSTR,
  XDWLWE_NOIFACE,
  XDWLWE_NOOUPUTBUF,
  XDWLWE_NOOUPUT,
  XDWLWE_NOIMG,
  XDWLWE_IMGOPEN,
  XDWLWE_IMGRESIZE,
  XDWLWE_IMGINVMODE,
  XDWLWE_NOFILE,
};

void xdwlw_error_set(enum xdwlw_errors errcode, const char *errmsg_fmt, ...);
void xdwlw_error_print();
char *xdwlw_error_get_msg();
enum xdwlw_errors xdwlw_error_get_code();

#endif
