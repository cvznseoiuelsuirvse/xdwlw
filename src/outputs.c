#include "xdwayland-client.h"
#include "xdwayland-core.h"
#include "xdwayland-utils.h"

#include "../xdg-output-unstable-v1-protocol.h"
#include "structs.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void handle_wl_output_mode(void *outputs, xdwl_arg *args);
void handle_xdg_output_logical_size(void *output, xdwl_arg *args);
void handle_xdg_output_name(void *output, xdwl_arg *args);

size_t get_outputs(xdwl_proxy *proxy, xdwl_list *globals, xdwl_list *outputs) {
  load_interfaces("/usr/share/wayland-protocols/unstable/xdg-output/"
                  "xdg-output-unstable-v1.xml");

  struct xdwl_output wl_output = {
      .mode = handle_wl_output_mode,
  };

  size_t n = ZXDG_OUTPUT_MANAGER_V1;
  for (xdwl_list *l = globals; l->next; l = l->next) {
    struct wl_global *g = l->data;
    size_t new_id = 0;

    if (STREQ(g->interface, "wl_output")) {
      new_id = object_register(proxy, 0, g->interface);

      xdwl_output_add_listener(proxy, &wl_output, outputs);

      struct output o = {.id = new_id, .fd = 0, .busy = false};
      xdwl_list_push(outputs, &o, sizeof(struct output));

    } else if (STREQ(g->interface, "zxdg_output_manager_v1")) {
      new_id = object_register(proxy, 0, g->interface);
      n &= ~ZXDG_OUTPUT_MANAGER_V1;
    }

    if (new_id > 0) {
      xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);
      xdwl_roundtrip(proxy);
    }

    free(g->interface);
  }

  struct xdzxdg_output_v1 zxdg_output_v1 = {.logical_size =
                                                handle_xdg_output_logical_size,
                                            .name = handle_xdg_output_name};

  for (xdwl_list *l = outputs; l->next; l = l->next) {
    struct output *o = l->data;
    size_t zxdg_output_object_id = object_register(proxy, 0, "zxdg_output_v1");

    xdzxdg_output_v1_add_listener(proxy, &zxdg_output_v1, o);
    xdzxdg_output_manager_v1_get_xdg_output(proxy, zxdg_output_object_id,
                                            o->id);

    xdwl_roundtrip(proxy);
  }

  return n;
};
