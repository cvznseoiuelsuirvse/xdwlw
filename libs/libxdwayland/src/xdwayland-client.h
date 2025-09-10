#ifndef XDWAYLAND_CLIENT_H
#define XDWAYLAND_CLIENT_H

#include "xdwayland-common.h"
#include "xdwayland-private.h"
#include "xdwayland-types.h"

xdwl_proxy *xdwl_proxy_create();
void xdwl_proxy_destroy(xdwl_proxy *proxy);

XDWL_MUST_CHECK int xdwl_roundtrip(xdwl_proxy *proxy);
XDWL_MUST_CHECK int xdwl_dispatch(xdwl_proxy *proxy);

/* error handling */
void xdwl_error_set(enum xdwl_errors errcode, const char *errmsg, ...);
void xdwl_error_print();
char *xdwl_error_get_msg();
enum xdwl_errors xdwl_error_get_code();
/* shouldn't be called within libxdwayland */
void xdwl_raise(xdwl_proxy *proxy);

int xdwl_destroy_listener(uint32_t object_id);

/* object registry */
struct xdwl_object *xdwl_object_get_by_id(xdwl_proxy *proxy,
                                          uint32_t object_id);
struct xdwl_object *xdwl_object_get_by_name(xdwl_proxy *proxy,
                                            const char *object_name);
XDWL_MUST_CHECK int xdwl_object_register(xdwl_proxy *proxy, uint32_t object_id,
                                         const char *object_name);
XDWL_MUST_CHECK int xdwl_object_unregister(xdwl_proxy *proxy,
                                           uint32_t object_id);
XDWL_MUST_CHECK int xdwl_object_unregister_last(xdwl_proxy *proxy,
                                                const char *object_name);

#endif
