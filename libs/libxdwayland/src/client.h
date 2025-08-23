#ifndef CLIENT_H
#define CLIENT_H

#include "structs.h"
xdwl_proxy *xdwl_proxy_create();
void xdwl_proxy_destroy(xdwl_proxy *proxy);

void xdwl_add_listener(xdwl_proxy *proxy, const char *object_name,
                       void *interface, void *user_data);
void xdwl_send_request(xdwl_proxy *proxy, char *object_name, size_t method_id,
                       int fd, size_t arg_count, ...);
void xdwl_recv_events(xdwl_proxy *proxy, xdwl_list *messages_list);

void xdwl_roundtrip(xdwl_proxy *proxy);
void xdwl_dispatch(xdwl_proxy *proxy);

#endif
