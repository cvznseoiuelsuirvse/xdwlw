#ifndef XDWAYLAND_TYPES_H
#define XDWAYLAND_TYPES_H

#include "xdwayland-collections.h"
#include "xdwayland-common.h"

typedef union xdwl_arg {
  uint32_t object_id;
  int32_t i;
  uint32_t u;
  float f;
  char *s;
  int fd;
} xdwl_arg;

typedef struct xdwl_proxy {
  int sockfd;
  xdwl_map *obj_reg;
} xdwl_proxy;

typedef struct xdwl_object {
  size_t id;
  char *name;
  const struct xdwl_interface *interface;
  uint32_t seq;
} xdwl_object;

enum xdwl_errors {
  XDWLERR_STD = 1,
  XDWLERR_ENV,
  XDWLERR_IDTAKEN,
  XDWLERR_NULLARG,
  XDWLERR_NULLOBJ,
  XDWLERR_NULLREQ,
  XDWLERR_NULLEVENT,
  XDWLERR_NULLIFACE,
  XDWLERR_NULLLISTNR,
  XDWLERR_SOCKCONN,
  XDWLERR_SOCKSEND,
  XDWLERR_SOCKRECV,
  XDWLERR_NOFREEBIT,
  XDWLERR_OUTOFRANGE,
  XDWLERR_NOPROTOXML,
};

#endif
