#ifndef XDWAYLAND_COMMON_H
#define XDWAYLAND_COMMON_H

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CAP 2048
#define MAX_ARGS 16
#define HEADER_SIZE 8
#define PADDED4(n) ((n + 4) & ~3)

#ifdef __GNUC__
#define __must_check __attribute__((warn_unused_result))
#else
#define __must_check
#endif

#endif
