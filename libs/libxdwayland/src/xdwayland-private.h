#ifndef XDWL_PRIVATE_H
#define XDWL_PRIVATE_H

#include "xdwayland-common.h"
#include "xdwayland-types.h"

struct xdwl_raw_message {
  uint32_t object_id;
  uint32_t method_id;
  size_t body_length;
  char *body;
  int fd;
};

struct xdwl_listener {
  void *event_handlers;
  void *user_data;
};

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

typedef void(xdwl_event_handler)(void *, xdwl_arg *);

int xdwl_send_request(xdwl_proxy *proxy, uint32_t object_id, char *object_name,
                      size_t method_id, size_t arg_count, ...);
int xdwl_add_listener(xdwl_proxy *proxy, const char *object_name,
                      void *event_handlers, size_t event_handlers_size,
                      void *user_data);

void xdwl_log(const char *level, const char *message, ...);
void xdwl_show_args(xdwl_arg *args, char *signature);

void xdwl_buf_write_u32(void *buffer, size_t *buf_size, uint32_t n);
void xdwl_buf_write_u16(void *buffer, size_t *buf_size, uint16_t n);
void xdwl_write_args(char *buffer, size_t *offset, xdwl_arg *args,
                     size_t arg_count, char *signature);

uint32_t xdwl_buf_read_u32(void *buffer, size_t *buf_size);
uint16_t xdwl_buf_read_u16(void *buffer, size_t *buf_size);
int xdwl_read_args(struct xdwl_raw_message *message, xdwl_arg *args,
                   const char *signature);
uint16_t xdwl_calculate_body_size(xdwl_arg *args, size_t arg_count,
                                  char *signature);

#endif
