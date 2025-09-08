#ifndef XDWAYLAND_TYPES_H
#define XDWAYLAND_TYPES_H

#include "xdwayland-collections.h"
#include "xdwayland-common.h"

typedef union xdwl_arg {
  int32_t i;
  uint32_t u;
  float f;
  char *s;
  int fd;
} xdwl_arg;

typedef struct xdwl_raw_message {
  uint32_t object_id;
  uint32_t method_id;
  size_t body_length;
  char body[CAP - HEADER_SIZE];
  int fd;
} xdwl_raw_message;

struct xdwl_listener {
  void *event_handlers;
  void *user_data;
};

typedef struct xdwl_proxy {
  int sockfd;
  char *buffer;
  xdwl_map *obj_reg;
} xdwl_proxy;

struct xdwl_method {
  char *name;
  size_t arg_count;
  char *signature;
};

struct xdwl_interface {
  char *name;
  const struct xdwl_method *requests;
  const struct xdwl_method *events;
};

typedef struct xdwl_object {
  size_t id;
  char *name;
  const struct xdwl_interface *interface;

} xdwl_object;

typedef void(xdwl_event_handler)(void *, xdwl_arg *);

enum xdwl_errors {
  XDWLERR_STD = 1,
  XDWLERR_ENV,
  XDWLERR_IDTAKEN,
  XDWLERR_NULLARG,
  XDWLERR_NULLOBJ,
  XDWLERR_NULLREQ,
  XDWLERR_NULLEVENT,
  XDWLERR_NULLINTF,
  XDWLERR_SOCKCONN,
  XDWLERR_SOCKSEND,
  XDWLERR_SOCKRECV,
  XDWLERR_NOFREEBIT,
  XDWLERR_OUTOFRANGE,
  XDWLERR_NOPROTOXML,
};

#endif
