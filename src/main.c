#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-structs.h"
#include "xdwayland-utils.h"

#include "structs.h"

#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define STREQ(s1, s2) strcmp(s1, s2) == 0

enum non_core_interfaces {
  WP_VIEWPORTER = 1,
  ZWLR_LAYER_SHELL_V1 = 1 << 1,
  ZXDG_OUTPUT_MANAGER_V1 = 1 << 2,
};

xdwl_proxy *proxy;

void _die(int n) {
  xdwl_proxy_destroy(proxy);
  printf("exited successfully\n");
  exit(0);
}

void show_not_supported_interfaces(size_t n) {
  if (n & ZWLR_LAYER_SHELL_V1) {
    printf("zwlr_layer_shell_v1 isn't supported by the compositor\n");
  } else if (n & WP_VIEWPORTER) {
    printf("wp_viewporter isn't supported by the compositor\n");
  } else if (n & ZXDG_OUTPUT_MANAGER_V1) {
    printf("zxdg_output_manager_v1 isn't supported by the compositor\n");
  }
}

void init_buffer() {};
void configure_surface() {};
void create_surface(xdwl_proxy *proxy) {
  uint32_t wl_surface_id = object_register(proxy, 0, "wl_surface");
  xdwl_compositor_create_surface(proxy, wl_surface_id);
};

void handle_global(void *data, xdwl_arg *args) {
  /* main
   * wl_shm, wl_compositor, zwlr_layer_shell_v1, wp_viewporter
   *
   * for outputs
   * wl_output, zxdg_output_manager_v1 */

  struct wl_global *global = malloc(sizeof(struct wl_global));

  global->name = args[0].u;
  global->version = args[2].u;

  size_t interface_len = strlen(args[1].s) + 1;
  global->interface = malloc(interface_len);
  memcpy(global->interface, args[1].s, interface_len);

  xdwl_list_append(data, global);
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

size_t bind_globals(xdwl_proxy *proxy) {
  xdwl_list *globals = xdwl_list_new();

  size_t wl_registry_id = object_register(proxy, 0, "wl_registry");

  struct xdwl_registry handlers = {.global = handle_global};
  xdwl_registry_add_listener(proxy, &handlers, globals);

  xdwl_display_get_registry(proxy, wl_registry_id);
  xdwl_roundtrip(proxy);

  load_interfaces(
      "/usr/share/wayland-protocols/stable/viewporter/viewporter.xml");
  load_interfaces("/usr/share/wayland-protocols/unstable/xdg-output/"
                  "xdg-output-unstable-v1.xml");
  load_interfaces("./protocols/wlr-layer-shell-unstable-v1.xml");

  size_t n = WP_VIEWPORTER | ZWLR_LAYER_SHELL_V1 | ZXDG_OUTPUT_MANAGER_V1;

  for (xdwl_list *l = globals; l; l = l->next) {
    struct wl_global *g = l->value;

    size_t new_id = 0;
    if (STREQ(g->interface, "wl_shm") || STREQ(g->interface, "wl_compositor") ||
        STREQ(g->interface, "wl_output")) {
      new_id = object_register(proxy, 0, g->interface);

    } else if (STREQ(g->interface, "wp_viewporter")) {
      new_id = object_register(proxy, 0, g->interface);
      n &= ~WP_VIEWPORTER;

    } else if (STREQ(g->interface, "zwlr_layer_shell_v1")) {
      new_id = object_register(proxy, 0, g->interface);
      n &= ~ZWLR_LAYER_SHELL_V1;

    } else if (STREQ(g->interface, "zxdg_output_manager_v1")) {
      new_id = object_register(proxy, 0, g->interface);
      n &= ~ZXDG_OUTPUT_MANAGER_V1;
    }

    if (new_id > 0) {
      xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);
      xdwl_roundtrip(proxy);
    }

    free(g->interface);
    free(g);
  }

  xdwl_list_destroy(globals);
  return n;
};

int main() {
  proxy = xdwl_proxy_create();
  signal(SIGINT, _die);

  object_register(proxy, 0, "wl_display");
  struct xdwl_display wl_display = {.delete_id = handle_delete_id,
                                    .error = handle_error};
  xdwl_display_add_listener(proxy, &wl_display, NULL);

  size_t binding_result = bind_globals(proxy);
  if (binding_result > 0) {
    show_not_supported_interfaces(binding_result);
    goto exit;
  };

exit:
  xdwl_proxy_destroy(proxy);
  return 0;
}
