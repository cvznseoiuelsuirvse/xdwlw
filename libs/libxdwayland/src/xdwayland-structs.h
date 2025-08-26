#ifndef XDWAYLAND_STRUCTS_H
#define XDWAYLAND_STRUCTS_H

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
  void *interface;
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
  xdwl_list *requests;
  xdwl_list *events;
};

typedef struct xdwl_object {
  size_t id;
  char *name;
  struct xdwl_interface *interface;

} xdwl_object;

typedef void(xdwl_event_handler)(void *, xdwl_arg *);

#endif
