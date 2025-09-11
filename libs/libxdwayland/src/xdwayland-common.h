#ifndef XDWAYLAND_COMMON_H
#define XDWAYLAND_COMMON_H

#include <assert.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CAP 4096
#define MAX_ARGS 16
#define HEADER_SIZE 8
#define PADDED4(n) ((n + 4) & ~3)

#ifdef __GNUC__
#define XDWL_MUST_CHECK __attribute__((warn_unused_result))
#else
#define XDWL_MUST_CHECK
#endif

#endif
