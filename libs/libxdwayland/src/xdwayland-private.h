#ifndef XDWL_PRIVATE_H
#define XDWL_PRIVATE_H

#include "xdwayland-common.h"
#include "xdwayland-types.h"

#include <libxml/parser.h>
#define TO_XMLSTRING(c) (xmlChar *)c

__must_check int xdwl_send_request(xdwl_proxy *proxy, char *object_name,
                                   size_t method_id, size_t arg_count, ...);
__must_check int xdwl_recv_events(xdwl_proxy *proxy, xdwl_list *messages_list);
__must_check int xdwl_add_listener(xdwl_proxy *proxy, const char *object_name,
                                   void *interface, void *user_data);

void xdwl_log(const char *level, const char *message, ...);
void xdwl_show_args(xdwl_arg *args, char *signature);

void xdwl_buf_write_u32(void *buffer, size_t *buf_size, uint32_t n);
void xdwl_buf_write_u16(void *buffer, size_t *buf_size, uint16_t n);
void xdwl_write_args(char *buffer, size_t *offset, xdwl_arg *args,
                     size_t arg_count, char *signature);

uint32_t xdwl_buf_read_u32(void *buffer, size_t *buf_size);
uint16_t xdwl_buf_read_u16(void *buffer, size_t *buf_size);
xdwl_arg *xdwl_read_args(xdwl_raw_message *message, const char *signature);
uint16_t xdwl_calculate_body_size(xdwl_arg *args, size_t arg_count,
                                  char *signature);

char *xdwl_parse_signature(xmlNodePtr cur);
__must_check int xdwl_parse_method(const char *interface_name, xmlNodePtr cur,
                                   xdwl_list *l);
__must_check int xdwl_parse_interface(xmlNodePtr cur, xdwl_map *m);

#endif
