#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "structs.h"
#include "xdwayland-utils.h"

extern xdwl_proxy *proxy;
void _log(const char *level, const char *message, ...);

void handle_zwlr_layer_surface_v1_configure(void *data, xdwl_arg *args) {
  uint32_t serial = args[0].u;
  xdzwlr_layer_surface_v1_ack_configure(proxy, serial);
};

void handle_wl_buffer_release(void *output, xdwl_arg *args) {
  ((struct output *)output)->busy = false;
  _log("INFO", "%s buffer released", ((struct output *)output)->name);
}

void handle_wl_registry_global(void *globals, xdwl_arg *args) {
  struct wl_global global = {.name = args[0].u, .version = args[2].u};

  size_t interface_len = strlen(args[1].s) + 1;
  global.interface = malloc(interface_len);
  memcpy(global.interface, args[1].s, interface_len);

  xdwl_list_push(globals, &global, sizeof(struct wl_global));
}

void handle_wl_display_error(void *data, xdwl_arg *args) {
  uint32_t object_id = args[0].u;
  uint32_t code = args[1].u;

  const char *message = args[2].s;
  const xdwl_object *object = object_get_by_id(proxy, object_id);
  const char *object_name = object->name;

  printf("%s#%d ERROR %d: %s\n", object_name, object_id, code, message);
}

void handle_wl_display_delete_id(void *data, xdwl_arg *args) {
  size_t object_id = args[0].u;
  object_unregister(proxy, object_id);
};

void handle_xdg_output_logical_size(void *output, xdwl_arg *args) {
  ((struct output *)output)->logical_width = args[0].i;
  ((struct output *)output)->logical_height = args[1].i;
};

void handle_xdg_output_name(void *output, xdwl_arg *args) {
  ((struct output *)output)->name = strdup(args[0].s);
};

void handle_wl_output_mode(void *outputs, xdwl_arg *args) {
  static size_t output_index = 0;
  struct output *o = xdwl_list_get(outputs, output_index);

  if (o) {
    o->width = args[1].i;
    o->height = args[2].i;
  } else {
    printf("warning: no output found with index %ld\n", output_index);
  }

  output_index++;
};
