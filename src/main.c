#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-structs.h"
#include "xdwayland-utils.h"

#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "outputs.h"
#include "structs.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define BPP 4

xdwl_proxy *proxy;

void handle_zwlr_layer_surface_v1_configure(void *data, xdwl_arg *args);
void handle_delete_id(void *data, xdwl_arg *args);
void handle_error(void *data, xdwl_arg *args);
void handle_global(void *globals, xdwl_arg *args);

void _abort(int n) {
  xdwl_proxy_destroy(proxy);
  printf("exited successfully\n");
  exit(n);
}

void _raise(const char *message, ...) {
  va_list args;

  va_start(args, message);

  fprintf(stderr, "error: ");
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
  xdwl_proxy_destroy(proxy);
  exit(errno);
}

void _log(const char *level, const char *message, ...) {
  va_list args;

  va_start(args, message);
  printf("%s: ", level);
  vprintf(message, args);

  va_end(args);
  fflush(stdout);
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

size_t bind_globals(xdwl_proxy *proxy, xdwl_list **globals) {
  /* main
   * wl_shm, wl_compositor, zwlr_layer_shell_v1, wp_viewporter
   *
   * for outputs
   * wl_output, zxdg_output_manager_v1 */

  size_t wl_registry_id = object_register(proxy, 0, "wl_registry");

  xdwl_registry_add_listener(
      proxy, &(struct xdwl_registry){.global = handle_global}, *globals);

  xdwl_display_get_registry(proxy, wl_registry_id);
  xdwl_roundtrip(proxy);

  load_interfaces(
      "/usr/share/wayland-protocols/stable/viewporter/viewporter.xml");
  load_interfaces("./protocols/wlr-layer-shell-unstable-v1.xml");

  size_t n = WP_VIEWPORTER | ZWLR_LAYER_SHELL_V1;

  size_t i;
  xdwl_list *l;
  for (l = *globals, i = 0; l->next; i++, l = l->next) {
    struct wl_global *g = l->data;

    size_t new_id = 0;
    if (STREQ(g->interface, "wl_shm") || STREQ(g->interface, "wl_compositor")) {
      new_id = object_register(proxy, 0, g->interface);

    } else if (STREQ(g->interface, "wp_viewporter")) {
      new_id = object_register(proxy, 0, g->interface);
      n &= ~WP_VIEWPORTER;

    } else if (STREQ(g->interface, "zwlr_layer_shell_v1")) {
      new_id = object_register(proxy, 0, g->interface);
      n &= ~ZWLR_LAYER_SHELL_V1;
    }

    if (new_id > 0) {
      xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);
      xdwl_roundtrip(proxy);
      xdwl_list_remove(globals, i);
    }
  }

  return n;
};

char *init_buffer(struct output *o, char *shm_name) {
  size_t wl_shm_pool_id = object_register(proxy, 0, "wl_shm_pool");
  size_t wl_buffer_id = object_register(proxy, 0, "wl_buffer");

  int32_t buffer_size = o->width * o->height * BPP;

  int fd_temp = shm_open(shm_name, O_RDWR | O_EXCL | O_CREAT, 0600);
  if (fd_temp == -1) {
    shm_unlink(shm_name);
    perror("shm_open");
    _raise("failed to initialize buffer. failed to shm_open");
  }

  if (ftruncate(fd_temp, buffer_size) == -1) {
    perror("ftruncate");
    _raise("failed to initialize buffer. failed to truncate shm to %d bytes",
           buffer_size);
  }

  xdwl_shm_create_pool(proxy, wl_shm_pool_id, fd_temp, buffer_size);

  xdwl_shm_pool_create_buffer(proxy, wl_buffer_id, 0, o->width, o->height,
                              o->width * BPP, XDWL_SHM_FORMAT_XRGB8888);

  char *buffer =
      mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_temp, 0);

  if (buffer == MAP_FAILED) {
    perror("mmap");
    _raise("failed to initialize buffer. failed to map memory");
  }

  return buffer;
}

void create_layer_surface(struct output *o) {
  size_t wl_surface_id = object_register(proxy, 0, "wl_surface");
  size_t zwlr_layer_surface_v1_id =
      object_register(proxy, 0, "zwlr_layer_surface_v1");

  xdwl_compositor_create_surface(proxy, wl_surface_id);
  xdzwlr_layer_shell_v1_get_layer_surface(
      proxy, zwlr_layer_surface_v1_id, wl_surface_id, o->id,
      XDZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, o->name);
}

void configure_layer_surface(struct output *o) {
  xdzwlr_layer_surface_v1_set_anchor(proxy, XDZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
  xdzwlr_layer_surface_v1_set_anchor(proxy,
                                     XDZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

  xdzwlr_layer_surface_v1_set_size(proxy, o->logical_width, o->logical_height);
  xdzwlr_layer_surface_v1_set_exclusive_zone(proxy, -1);

  xdwl_surface_commit(proxy);
  xdwl_roundtrip(proxy);
};

char *setup_output(struct output *o, char *shm_name) {
  create_layer_surface(o);

  xdzwlr_layer_surface_v1_add_listener(
      proxy,
      &(struct xdzwlr_layer_surface_v1){
          .configure = handle_zwlr_layer_surface_v1_configure},
      NULL);

  configure_layer_surface(o);

  return init_buffer(o, shm_name);
}

void draw_color_on_output(struct output *o, uint32_t color) {
  char name[255];
  snprintf(name, 255, "/%s", o->name);
  shm_unlink(name);

  uint32_t *buffer = (uint32_t *)setup_output(o, name);
  int color_bytes[] = COLOR(color);

  for (size_t i = 0; i < o->width * o->height; i++) {
    buffer[i] = color;
  }

  xdwl_object *wl_buffer = object_get_by_name(proxy, "wl_buffer");
  if (wl_buffer == NULL)
    _raise("no wl_buffer found");

  size_t wl_buffer_id = wl_buffer->id;

  xdwl_surface_attach(proxy, wl_buffer_id, 0, 0);
  xdwl_surface_commit(proxy);

  xdwl_roundtrip(proxy);
};

void draw_image_on_output(struct output *o, char *image_path) {};

int main() {
  proxy = xdwl_proxy_create();
  signal(SIGINT, _abort);

  object_register(proxy, 0, "wl_display");
  struct xdwl_display wl_display = {.delete_id = handle_delete_id,
                                    .error = handle_error};
  xdwl_display_add_listener(proxy, &wl_display, NULL);

  xdwl_list *globals = xdwl_list_new();
  xdwl_list *outputs = xdwl_list_new();
  size_t binding_result = 0;

  binding_result |= bind_globals(proxy, &globals);
  binding_result |= get_outputs(proxy, globals, outputs);

  xdwl_list_destroy(globals);

  if (binding_result > 0) {
    show_not_supported_interfaces(binding_result);
    goto exit;
  };

  for (xdwl_list *l = outputs; l->next; l = l->next) {
    struct output *o = l->data;
    _log("INFO", "Found an output: %s %dx%d(%dx%d)\n", o->name, o->width,
         o->height, o->logical_width, o->logical_height);

    draw_color_on_output(o, 0xff0000);
  }

  for (xdwl_list *l = outputs; l->next; l = l->next) {
    struct output *o = l->data;
    free(o->name);
  }
  xdwl_list_destroy(outputs);

  while (1)
    xdwl_dispatch(proxy);

exit:
  xdwl_proxy_destroy(proxy);
  return 0;
}
