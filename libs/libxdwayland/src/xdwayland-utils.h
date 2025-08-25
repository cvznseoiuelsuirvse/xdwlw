#ifndef XDWAYLAND_UTILS_H
#define XDWAYLAND_UTILS_H

#include "xdwayland-common.h"
#include "xdwayland-proto-parser.h"
#include "xdwayland-structs.h"

extern xdwl_map *interfaces;
static inline void load_interfaces(const char *xml_path) {
  parse(xml_path, interfaces);
}

void xdwl_log(const char *level, const char *message, ...);
void xdwl_show_args(xdwl_arg *args, char *signature);
void xdwl_raise(xdwl_proxy *proxy, const char *origin, const char *message,
                ...);

void buf_write_u32(void *buffer, size_t *buf_size, uint32_t n);
void buf_write_u16(void *buffer, size_t *buf_size, uint16_t n);
void write_args(char *buffer, size_t *offset, xdwl_arg *args, size_t arg_count,
                char *signature);

uint32_t buf_read_u32(void *buffer, size_t *buf_size);
uint16_t buf_read_u16(void *buffer, size_t *buf_size);
char *buf_read_string(void *buffer, size_t *buf_size);
xdwl_arg *read_args(xdwl_raw_message *message, const char *signature);

uint16_t calculate_body_size(xdwl_arg *args, size_t arg_count, char *signature);

xdwl_object *object_get_by_id(xdwl_proxy *proxy, size_t object_id);
xdwl_object *object_get_by_name(xdwl_proxy *proxy, const char *object_name);

size_t object_register(xdwl_proxy *proxy, size_t object_id,
                       const char *object_name);
void object_unregister(xdwl_proxy *proxy, size_t object_id);

#endif
