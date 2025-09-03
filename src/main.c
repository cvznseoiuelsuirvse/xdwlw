#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-structs.h"
#include "xdwayland-utils.h"
#include <linux/limits.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "outputs.h"
#include "structs.h"

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>

#define BPP 4
#define LERP(v0, v1, t) ((1 - t) * v0 + t * v1);
#define COLOR(hex)                                                             \
  {((hex >> 24) & 0xFF), ((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF),            \
   (hex & 0xFF)}

enum modes { CENTER = 'C', FIT = 'F' };
xdwl_proxy *proxy;
xdwl_list *global_listeners;

void handle_zwlr_layer_surface_v1_configure(void *data, xdwl_arg *args);
void handle_wl_display_delete_id(void *data, xdwl_arg *args);
void handle_wl_display_error(void *data, xdwl_arg *args);
void handle_wl_registry_global(void *globals, xdwl_arg *args);
void handle_wl_buffer_release(void *globals, xdwl_arg *args);

void _abort(int n) {
  xdwl_list_destroy(global_listeners);
  xdwl_proxy_destroy(proxy);
  printf("\n:P\n");
  exit(n);
}

void _raise(const char *message, ...) {
  va_list args;

  va_start(args, message);

  fprintf(stderr, "error: ");
  vfprintf(stderr, message, args);
  fprintf(stderr, "\n");

  va_end(args);
  _abort(errno);
}

void _log(const char *level, const char *message, ...) {
  va_list args;

  va_start(args, message);
  printf("%s: ", level);
  vprintf(message, args);
  printf("\n");

  va_end(args);
  fflush(stdout);
}

uint32_t *resize_image_nni(uint32_t *i_buffer, size_t i_width, size_t i_height,
                           size_t o_width, size_t o_height) {

  uint32_t *o_buffer = malloc(o_width * o_height * BPP);
  if (!o_buffer)
    return NULL;

  for (size_t y = 0; y < o_height; y++) {
    for (size_t x = 0; x < o_width; x++) {
      int src_x = round(x * ((float)i_width / o_width));
      int src_y = round(y * ((float)i_height / o_height));
      src_x = fmin(src_x, i_width - 1);
      src_y = fmin(src_y, i_height - 1);

      uint32_t pixel = i_buffer[src_y * i_width + src_x];
      uint8_t pixelb[] = COLOR(pixel);

      uint8_t a = pixelb[0];
      uint8_t r = pixelb[1];
      uint8_t g = pixelb[2];
      uint8_t b = pixelb[3];

      o_buffer[y * o_width + x] = a << 24 | b << 16 | g << 8 | r;
    }
  }

  return o_buffer;
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
      proxy, &(struct xdwl_registry){.global = handle_wl_registry_global},
      *globals);

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

char *init_buffer(struct output *o) {
  if (o->busy == true) {
    return NULL;
  }

  char name[255];
  snprintf(name, 255, "xdwlw-shm-%x", rand());
  size_t wl_shm_pool_id = object_register(proxy, 0, "wl_shm_pool");
  size_t wl_buffer_id = object_register(proxy, 0, "wl_buffer");

  struct xdwl_buffer *i =
      xdwl_list_push(global_listeners,
                     &(struct xdwl_buffer){.release = handle_wl_buffer_release},
                     sizeof(struct xdwl_buffer));
  xdwl_buffer_add_listener(proxy, i, o);

  int32_t buffer_size = o->width * o->height * BPP;

  int fd = shm_open(name, O_RDWR | O_EXCL | O_CREAT, 0600);
  shm_unlink(name);

  if (fd == -1) {
    perror("shm_open");
    _raise("failed to initialize buffer. failed to shm_open");
  }

  if (ftruncate(fd, buffer_size) == -1) {
    perror("ftruncate");
    _raise("failed to initialize buffer. failed to truncate shm to %d bytes",
           buffer_size);
  }

  xdwl_shm_create_pool(proxy, wl_shm_pool_id, fd, buffer_size);

  xdwl_shm_pool_create_buffer(proxy, wl_buffer_id, 0, o->width, o->height,
                              o->width * BPP, XDWL_SHM_FORMAT_ARGB8888);

  o->busy = true;
  o->fd = fd;

  _log("INFO", "Initialized buffer %s with a size of %d", name, buffer_size);

  char *buffer =
      mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

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

  struct xdzwlr_layer_surface_v1 *i =
      xdwl_list_push(global_listeners,
                     &(struct xdzwlr_layer_surface_v1){
                         .configure = handle_zwlr_layer_surface_v1_configure},
                     sizeof(struct xdzwlr_layer_surface_v1));
  xdzwlr_layer_surface_v1_add_listener(proxy, i, NULL);

  xdwl_compositor_create_surface(proxy, wl_surface_id);
  xdzwlr_layer_shell_v1_get_layer_surface(
      proxy, zwlr_layer_surface_v1_id, wl_surface_id, o->id,
      XDZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, o->name);

  _log("INFO", "Created layer surface#%d for %s", zwlr_layer_surface_v1_id,
       o->name);
}

void configure_layer_surface(struct output *o) {
  xdzwlr_layer_surface_v1_set_anchor(proxy, XDZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
  xdzwlr_layer_surface_v1_set_anchor(proxy,
                                     XDZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

  xdzwlr_layer_surface_v1_set_size(proxy, o->logical_width, o->logical_height);
  xdzwlr_layer_surface_v1_set_exclusive_zone(proxy, -1);

  _log("INFO", "Set %s layer surface size to: %dx%d", o->name, o->logical_width,
       o->logical_height);

  xdwl_surface_commit(proxy);
  xdwl_roundtrip(proxy);
};

char *setup_output(struct output *o) {
  create_layer_surface(o);
  configure_layer_surface(o);
  return init_buffer(o);
}

void attach_and_commit() {
  xdwl_object *wl_buffer = object_get_by_name(proxy, "wl_buffer");
  if (wl_buffer == NULL)
    _raise("no wl_buffer found");

  size_t wl_buffer_id = wl_buffer->id;

  xdwl_surface_attach(proxy, wl_buffer_id, 0, 0);
  xdwl_surface_commit(proxy);

  xdwl_roundtrip(proxy);
}

void draw_gradient_on_output(struct output *o, uint32_t c0, uint32_t c1) {
  uint32_t *buffer = (uint32_t *)setup_output(o);
  if (buffer == NULL) {
    _log("ERROR", "Can't write to %s buffer. %s is busy", o->name);
    return;
  }

  int c0_b[] = COLOR(c0);
  int c1_b[] = COLOR(c1);

  for (size_t y = 0; y < o->height; y++) {
    for (size_t x = 0; x < o->width; x++) {
      float t = (float)x / o->width;

      char r = LERP(c0_b[1], c1_b[1], t);
      char g = LERP(c0_b[2], c1_b[2], t);
      char b = LERP(c0_b[3], c1_b[3], t);

      uint32_t pixel = r << 16 | g << 8 | b;
      buffer[y * o->width + x] = pixel;
    }
  }

  attach_and_commit();

  _log("INFO", "Set new buffer gradient");
}
void draw_color_on_output(struct output *o, uint32_t color) {
  uint32_t *buffer = (uint32_t *)setup_output(o);
  if (buffer == NULL) {
    _log("ERROR", "Can't write to %s buffer. %s is busy", o->name);
    return;
  }

  for (size_t i = 0; i < o->width * o->height; i++) {
    buffer[i] = color;
  }

  attach_and_commit();

  int color_bytes[] = COLOR(color);
  _log("INFO", "Set new buffer color to #%02x%02x%02x%02x", color_bytes[1],
       color_bytes[2], color_bytes[3], color_bytes[0]);
};

void draw_image_on_output(struct output *o, char *image_path, enum modes mode) {
  uint32_t *buffer = (uint32_t *)setup_output(o);
  if (buffer == NULL) {
    _log("ERROR", "Can't write to %s buffer. %s is busy", o->name);
    return;
  }

  int iw;
  int ih;
  int channels;

  uint32_t *pixels = (uint32_t *)stbi_load(image_path, &iw, &ih, &channels, 4);

  if (!pixels) {
    _log("ERROR", "Failed to read %s", image_path);
    return;
  }

  _log("INFO", "Loaded %s: %dx%d channels: %d", image_path, iw, ih, channels);

  uint32_t *resized = resize_image_nni(pixels, iw, ih, o->width, o->height);
  if (!resized) {
    _log("ERROR", "Failed to resize %s from %dx%d to %dx%d", image_path, iw, ih,
         o->width, o->height);

    stbi_image_free(pixels);
    return;
  }

  memcpy(buffer, resized, o->width * o->height * BPP);

  free(resized);
  stbi_image_free(pixels);

  attach_and_commit();
};

xdwl_list *init() {
  global_listeners = xdwl_list_new();
  if (!global_listeners)
    return NULL;

  proxy = xdwl_proxy_create();
  if (!proxy)
    return NULL;

  signal(SIGINT, _abort);

  object_register(proxy, 0, "wl_display");
  struct xdwl_display *wl_display = xdwl_list_push(
      global_listeners,
      &(struct xdwl_display){.delete_id = handle_wl_display_delete_id,
                             .error = handle_wl_display_error},
      sizeof(struct xdwl_display));

  xdwl_display_add_listener(proxy, wl_display, NULL);

  xdwl_list *globals = xdwl_list_new();
  if (!globals)
    return NULL;

  xdwl_list *outputs = xdwl_list_new();
  if (!outputs)
    return NULL;

  size_t binding_result = 0;

  binding_result |= bind_globals(proxy, &globals);
  binding_result |= get_outputs(proxy, globals, outputs);

  xdwl_list_destroy(globals);

  if (binding_result > 0) {
    show_not_supported_interfaces(binding_result);
    return NULL;
  };

  return outputs;
}

int main(int argc, char **argv) {
  if (argc < 2)
    return 1;

  int opt;
  const char *rpath = NULL;
  char apath[PATH_MAX];
  enum modes mode = 0;

  while ((opt = getopt(argc, argv, "i:m:")) != -1) {
    switch (opt) {
    case 'i':
      rpath = optarg;
      break;
    case 'm':
      mode = *optarg;
      break;
    }
  }

  if (rpath == NULL || mode == 0)
    return 1;

  if (!realpath(rpath, apath)) {
    perror("realpath");
    return 1;
  }

  xdwl_list *outputs = init();
  if (!outputs) {
    _log("ERROR", "Failed to to init");
    _abort(errno);
  }

  for (; outputs->next; outputs = outputs->next) {
    struct output *o = outputs->data;
    _log("INFO", "Found an output: %s %dx%d", o->name, o->width, o->height,
         o->logical_width, o->logical_height);

    draw_image_on_output(o, apath, mode);
  }

  for (xdwl_list *l = outputs; l->next; l = l->next) {
    struct output *o = l->data;
    free(o->name);
  }
  xdwl_list_destroy(outputs);

  while (1)
    xdwl_dispatch(proxy);
}
