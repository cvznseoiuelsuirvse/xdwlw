#include "../xdg-output-unstable-v1-protocol.h"
#include "xdwayland-client.h"
#include "xdwayland-core.h"

#include "xdwlw-common.h"
#include "xdwlw-error.h"
#include "xdwlw-types.h"

#define ensure_res1(n)                                                         \
  if ((n) == NULL)                                                             \
  return -1

#define ensure_res2(n)                                                         \
  if ((n) == -1)                                                               \
  return -1

extern void xdwlw_exit();
void handle_wl_output_mode(void *outputs, xdwl_arg *args);
void handle_xdg_output_logical_size(void *output, xdwl_arg *args);
void handle_xdg_output_name(void *output, xdwl_arg *args);

int get_outputs(xdwl_proxy *proxy, xdwl_list *globals, xdwl_list *outputs) {
  xdwl_list *l;
  struct wl_global *g;
  struct output *o;

  struct xdwl_output_event_handlers wl_output = {
      .mode = handle_wl_output_mode,
  };

  size_t n = ZXDG_OUTPUT_MANAGER_V1;

  xdwl_list_for_each(l, globals, g) {
    int new_id = 0;

    if (STREQ(g->interface, "wl_output")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      if (new_id == -1) {
        xdwlw_error_set(XDWLWE_NOINTF, "failed to register %s", g->interface);
        xdwlw_exit();
      }
      ensure_res2(xdwl_output_add_listener(proxy, &wl_output, outputs));

      struct output o = {.id = new_id, .buffer = NULL, .busy = 0};
      ensure_res1(xdwl_list_push(outputs, &o, sizeof(struct output)));

    } else if (STREQ(g->interface, "zxdg_output_manager_v1")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      if (new_id == -1) {
        xdwlw_error_set(XDWLWE_NOINTF, "failed to register %s", g->interface);
        xdwlw_exit();
      }
      n &= ~ZXDG_OUTPUT_MANAGER_V1;
    }

    if (new_id > 0) {
      xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);
      ensure_res2(xdwl_roundtrip(proxy));
    }

    free(g->interface);
  }

  struct xdzxdg_output_v1_event_handlers zxdg_output_v1 = {
      .logical_size = handle_xdg_output_logical_size,
      .name = handle_xdg_output_name};

  xdwl_list_for_each(l, outputs, o) {
    size_t zxdg_output_object_id =
        xdwl_object_register(proxy, 0, "zxdg_output_v1");

    ensure_res2(xdzxdg_output_v1_add_listener(proxy, &zxdg_output_v1, o));
    xdzxdg_output_manager_v1_get_xdg_output(proxy, zxdg_output_object_id,
                                            o->id);

    ensure_res2(xdwl_roundtrip(proxy));
  }

  return n;
};
