#include "client.h"
#include "collections.h"
#include "core.h"
#include "structs.h"
#include "utils.h"

#include <signal.h>
#include <stddef.h>
#include <stdint.h>

#define STREQ(s1, s2) strcmp(s1, s2) == 0

xdwl_proxy *proxy;

struct wl_global {
  uint32_t name;
  char *interface;
  uint32_t version;
};

void _die(int n) {
  xdwl_proxy_destroy(proxy);
  printf("exited successfully\n");
  exit(0);
}

void init_buffer() {};
void configure_surface() {};
void create_surface(xdwl_proxy *proxy) {
  uint32_t wl_surface_id = object_register(proxy, 0, "wl_surface");
  xdwl_compositor_create_surface(proxy, wl_surface_id);
};

void handle_global(void *data, xdwl_arg *args) {
  uint32_t name = args[0].u;
  char *interface = args[1].s;
  uint32_t version = args[2].u;

  if (STREQ(interface, "wl_shm") || STREQ(interface, "wl_compositor") ||
      STREQ(interface, "zwlr_layer_shell_v1") ||
      STREQ(interface, "wp_viewporter")) {

    struct wl_global *global = malloc(sizeof(struct wl_global));

    global->name = name;
    global->interface = malloc(strlen(interface));
    strcpy(global->interface, interface);
    global->version = version;

    xdwl_list_append(data, global);
  }
}

void handle_error(void *data, xdwl_arg *args) {
  uint32_t object_id = args[0].u;
  uint32_t code = args[1].u;

  const char *message = args[2].s;
  const xdwl_object *object = object_get_by_id(proxy, object_id);
  const char *object_name = object->name;

  printf("%s#%d ERROR %d: %s\n", object_name, object_id, code, message);
}

void handle_delete_id(void *data, xdwl_arg *args) {
  size_t object_id = args[0].u;
  object_unregister(proxy, object_id);
};

void bind_globals(xdwl_proxy *proxy) {
  /* main
   * wl_shm, wl_compositor, zwlr_layer_shell_v1, wp_viewporter
   *
   * for outputs
   * wl_output, zxdg_output_manager_v1 */

  xdwl_list *globals = xdwl_list_new();

  size_t wl_registry_id = object_register(proxy, 0, "wl_registry");

  struct xdwl_registry handlers = {.global = handle_global};
  xdwl_registry_add_listener(proxy, &handlers, globals);

  xdwl_display_get_registry(proxy, wl_registry_id);
  xdwl_roundtrip(proxy);

  load_interfaces("./protocols/viewporter.xml");
  load_interfaces("./protocols/wlr-layer-shell-unstable-v1.xml");

  for (xdwl_list *l = globals; l; l = l->next) {
    struct wl_global *g = l->value;

    size_t new_id = object_register(proxy, 0, g->interface);
    xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);

    xdwl_roundtrip(proxy);

    free(g->interface);
    free(g);
  }

  xdwl_list_destroy(globals);
};

int main() {
  proxy = xdwl_proxy_create();
  signal(SIGINT, _die);

  object_register(proxy, 0, "wl_display");
  struct xdwl_display wl_display = {.delete_id = handle_delete_id,
                                    .error = handle_error};
  xdwl_display_add_listener(proxy, &wl_display, NULL);

  bind_globals(proxy);

  xdwl_proxy_destroy(proxy);
  return 0;
}
