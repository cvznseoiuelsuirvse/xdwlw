#ifndef XDWAYLAND_CLIENT_H
#define XDWAYLAND_CLIENT_H

#include "xdwayland-common.h"
#include "xdwayland-types.h"

xdwl_proxy *xdwl_proxy_create();
void xdwl_proxy_destroy(xdwl_proxy *proxy);

__must_check int xdwl_roundtrip(xdwl_proxy *proxy);
__must_check int xdwl_dispatch(xdwl_proxy *proxy);

/* error handling */
void xdwl_error_set(enum xdwl_errors errcode, const char *errmsg, ...);
/* shouldn't be called within libxdwayland */
void xdwl_error_print();
char *xdwl_error_get_msg();
enum xdwl_errors xdwl_error_get_code();
void xdwl_raise(xdwl_proxy *proxy);

__must_check int xdwl_load_interfaces(const char *xml_path);

/* object registry */
xdwl_object *xdwl_object_get_by_id(xdwl_proxy *proxy, uint32_t object_id);
xdwl_object *xdwl_object_get_by_name(xdwl_proxy *proxy,
                                     const char *object_name);
__must_check int xdwl_object_register(xdwl_proxy *proxy, uint32_t object_id,
                                      const char *object_name);
__must_check int xdwl_object_unregister(xdwl_proxy *proxy, uint32_t object_id);
__must_check int xdwl_object_unregister_last(xdwl_proxy *proxy,
                                             const char *object_name);

#endif
