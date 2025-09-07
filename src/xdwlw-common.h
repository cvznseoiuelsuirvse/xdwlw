#ifndef XDWLW_COMMON_H
#define XDWLW_COMMON_H

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define BPP 4

static void xdwlw_log(const char *level, const char *message, ...) {
  va_list args;

  va_start(args, message);
  printf("[%s] xdwlw: ", level);
  vprintf(message, args);
  printf("\n");

  va_end(args);
  fflush(stdout);
}

#endif
