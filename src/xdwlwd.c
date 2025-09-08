#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-types.h"
#include "xdwlw-common.h"
#include <stdint.h>
#include <sys/mman.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "xdwlw-error.h"
#include "xdwlw-ipc.h"
#include "xdwlw-outputs.h"
#include "xdwlw-types.h"

#include <fcntl.h>

#define LERP(v0, v1, t) ((1 - t) * v0 + t * v1);
#define COLOR(hex)                                                             \
  {((hex >> 24) & 0xFF), ((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF),            \
   (hex & 0xFF)}

#define ensure_result(n)                                                       \
  if ((n) == -1)                                                               \
  xdwlw_exit()

int sfd;
xdwl_proxy *proxy;
xdwl_list *global_listeners;

void create_output_buffer(struct output *o);
void destroy_output_buffer(struct output *o);

void xdwlw_exit() {
  int xdwl_err = xdwl_error_get_code();
  int xdwlw_err = xdwlw_error_get_code();

  if (xdwl_err != 0)
    xdwl_raise(proxy);

  if (xdwlw_err != 0)
    xdwlw_error_print();

  if (ipc_server_close(sfd) == -1) {
    xdwlw_error_set(XDWLWE_DCLOSE, "xdwlwd: failed to close socket");
    xdwlw_error_print();
  }

  xdwl_list_destroy(global_listeners);
  exit(xdwl_err || xdwlw_err);
}

void handle_zwlr_layer_surface_v1_configure(void *output, xdwl_arg *args) {
  struct output *o = output;

  uint32_t serial = args[0].u;
  uint32_t width = args[1].u;
  uint32_t height = args[2].u;

  xdzwlr_layer_surface_v1_ack_configure(proxy, serial);

  if (o->width != width || o->height != height) {
    destroy_output_buffer(o);

    o->width = width;
    o->height = height;
  }

  if (o->buffer == NULL) {
    create_output_buffer(o);
  }
}

void handle_wl_buffer_release(void *output, xdwl_arg *_) {
  struct output *o = output;
  xdwlw_log("info", "%s buffer released", o->name);
  // destroy_output_buffer((struct output *)output);
}

void handle_wl_registry_global(void *globals, xdwl_arg *args) {
  struct wl_global global = {.name = args[0].u, .version = args[2].u};

  size_t interface_len = strlen(args[1].s) + 1;
  global.interface = malloc(interface_len);
  memcpy(global.interface, args[1].s, interface_len);

  assert(xdwl_list_push(globals, &global, sizeof(struct wl_global)) != NULL);
}

void handle_wl_display_error(void *_, xdwl_arg *args) {
  uint32_t object_id = args[0].u;
  uint32_t code = args[1].u;

  const char *message = args[2].s;
  const xdwl_object *object = xdwl_object_get_by_id(proxy, object_id);
  const char *object_name = object->name;

  printf("%s#%d ERROR %d: %s\n", object_name, object_id, code, message);
}

void handle_wl_display_delete_id(void *_, xdwl_arg *args) {
  size_t object_id = args[0].u;
  assert(xdwl_object_unregister(proxy, object_id) == 0);
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
uint32_t *resize_image_nni(uint32_t *i_buffer, size_t i_width, size_t i_height,
                           size_t o_width, size_t o_height) {

  uint32_t *o_buffer = malloc(o_width * o_height * BPP);
  if (!o_buffer) {
    perror("malloc");
    return NULL;
  }

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

  size_t wl_registry_id = xdwl_object_register(proxy, 0, "wl_registry");

  struct xdwl_registry_event_handlers *wl_registry =
      xdwl_list_push(global_listeners,
                     &(struct xdwl_registry_event_handlers){
                         .global = handle_wl_registry_global},
                     sizeof(struct xdwl_registry_event_handlers));

  ensure_result(xdwl_registry_add_listener(proxy, wl_registry, *globals));

  xdwl_display_get_registry(proxy, wl_registry_id);
  ensure_result(xdwl_roundtrip(proxy));

  size_t n = WP_VIEWPORTER | ZWLR_LAYER_SHELL_V1;

  size_t i;
  xdwl_list *l;
  for (l = *globals, i = 0; l->next; i++, l = l->next) {
    struct wl_global *g = l->data;

    int new_id = 0;
    if (STREQ(g->interface, "wl_shm") || STREQ(g->interface, "wl_compositor")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      if (new_id == -1) {
        xdwlw_error_set(XDWLWE_NOINTF, "failed to register %s", g->interface);
        xdwlw_exit();
      }

    } else if (STREQ(g->interface, "wp_viewporter")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      if (new_id == -1) {
        xdwlw_error_set(XDWLWE_NOINTF, "failed to register %s", g->interface);
        xdwlw_exit();
      }

      n &= ~WP_VIEWPORTER;

    } else if (STREQ(g->interface, "zwlr_layer_shell_v1")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      n &= ~ZWLR_LAYER_SHELL_V1;
    }

    if (new_id > 0) {
      xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);
      ensure_result(xdwl_roundtrip(proxy));
      xdwl_list_remove(globals, i);
    }
  }

  return n;
};

void commit(struct output *o) {
  xdwl_object *wl_buffer_object = xdwl_object_get_by_name(proxy, "wl_buffer");
  if (!wl_buffer_object) {
    xdwlw_exit();
  }

  uint32_t wl_buffer_id = wl_buffer_object->id;
  xdwl_surface_attach(proxy, wl_buffer_id, 0, 0);
  xdwl_surface_damage_buffer(proxy, 0, 0, o->width * BPP, o->height);
  xdwl_surface_commit(proxy);
  ensure_result(xdwl_roundtrip(proxy));
}

int draw_color_on_output(struct output *o, uint32_t color) {
  for (size_t i = 0; i < o->width * o->height; i++) {
    o->buffer[i] = color;
  }

  int color_bytes[] = COLOR(color);
  xdwlw_log("info", "set new buffer color to #%02x%02x%02x%02x", color_bytes[1],
            color_bytes[2], color_bytes[3], color_bytes[0]);

  commit(o);
  return 0;
};

int draw_image_on_output(struct output *o) {
  int iw, ih, channels;

  uint32_t *pixels =
      (uint32_t *)stbi_load(o->image_path, &iw, &ih, &channels, 4);

  if (!pixels) {
    xdwlw_error_set(XDWLWE_IMGOPEN, "failed to open %s", o->image_path);
    return 1;
  }
  xdwlw_log("info", "loaded %s: %dx%d channels: %d", o->image_path, iw, ih,
            channels);

  float sx, sy, scale;
  size_t nw, nh, left, top, right, bottom;

  uint32_t *resized;

  switch (o->image_mode) {
  case IMAGE_MODE_CENTER:
    sx = (float)o->width / iw;
    sy = (float)o->height / ih;

    scale = fmax(sx, sy);
    nw = iw * scale;
    nh = ih * scale;

    left = floor((nw - (float)o->width) / 2);
    top = floor((nh - (float)o->height) / 2);

    right = left + o->width;
    bottom = top + o->height;

    resized = resize_image_nni(pixels, iw, ih, nw, nh);
    if (!resized) {
      xdwlw_error_set(XDWLWE_IMGRESIZE,
                      "failed to resize %s from %dx%d to %dx%d", o->image_path,
                      iw, ih, o->width, o->height);

      stbi_image_free(pixels);
      return 1;
    }

    size_t i = 0;
    for (size_t y = top; y < bottom; y++) {
      for (size_t x = left; x < right; x++) {
        o->buffer[i++] = resized[y * nw + x];
      }
    }

    break;

  case IMAGE_MODE_FIT:
    resized = resize_image_nni(pixels, iw, ih, o->width, o->height);
    if (!resized) {
      xdwlw_error_set(XDWLWE_IMGRESIZE,
                      "failed to resize %s from %dx%d to %dx%d", o->image_path,
                      iw, ih, o->width, o->height);

      stbi_image_free(pixels);
      return 1;
    }

    memcpy(o->buffer, resized, o->width * o->height * BPP);
    break;

  default:
    xdwlw_error_set(XDWLWE_IMGINVMODE, "invalid image mode: %c", o->image_mode);
    return 1;
  }

  free(resized);
  stbi_image_free(pixels);

  commit(o);
  xdwlw_log("info", "set %s on %s. mode: %c", o->image_path, o->name,
            o->image_mode);

  return 0;
};

xdwl_list *init() {
  global_listeners = xdwl_list_new();
  if (!global_listeners)
    return NULL;

  proxy = xdwl_proxy_create();
  if (!proxy)
    return NULL;

  ensure_result(xdwl_object_register(proxy, 0, "wl_display"));
  struct xdwl_display_event_handlers *wl_display =
      xdwl_list_push(global_listeners,
                     &(struct xdwl_display_event_handlers){
                         .delete_id = handle_wl_display_delete_id,
                         .error = handle_wl_display_error},
                     sizeof(struct xdwl_display_event_handlers));

  ensure_result(xdwl_display_add_listener(proxy, wl_display, NULL));

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

void create_output_buffer(struct output *o) {
  /* create buffer */
  size_t wl_shm_pool_id;
  size_t wl_buffer_id;

  char name[255];
  snprintf(name, 255, "xdwlw-shm-%x", rand());

  int32_t buffer_size = o->width * o->height * BPP;

  int fd = shm_open(name, O_RDWR | O_EXCL | O_CREAT, 0600);
  shm_unlink(name);

  if (fd == -1) {
    perror("shm_open");
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create %s output buffer",
                    o->name);
    xdwlw_exit();
  }

  if (ftruncate(fd, buffer_size) == -1) {
    perror("ftruncate");
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create %s output buffer",
                    o->name);
    xdwlw_exit();
  }

  struct xdwl_object *wl_shm_pool_object =
      xdwl_object_get_by_name(proxy, "wl_shm_pool");

  if (wl_shm_pool_object == NULL) {
    wl_shm_pool_id = xdwl_object_register(proxy, 0, "wl_shm_pool");
    xdwl_shm_create_pool(proxy, wl_shm_pool_id, fd, buffer_size);
  } else {
    wl_shm_pool_id = wl_shm_pool_object->id;
  }

  wl_buffer_id = xdwl_object_register(proxy, 0, "wl_buffer");
  struct xdwl_buffer_event_handlers *wl_buffer = xdwl_list_push(
      global_listeners,
      &(struct xdwl_buffer_event_handlers){.release = handle_wl_buffer_release},
      sizeof(struct xdwl_buffer_event_handlers));
  ensure_result(xdwl_buffer_add_listener(proxy, wl_buffer, o));

  xdwl_shm_pool_create_buffer(proxy, wl_buffer_id, 0, o->width, o->height,
                              o->width * BPP, XDWL_SHM_FORMAT_ARGB8888);

  uint32_t *buffer =
      mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (buffer == MAP_FAILED) {
    perror("mmap");
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create %s output buffer",
                    o->name);
    xdwlw_exit();
  }

  xdwlw_log("info", "initialized buffer %s with a size of %d", name,
            buffer_size);

  o->buffer = buffer;
}

void destroy_output_buffer(struct output *o) {
  if (!o)
    return;

  munmap(o->buffer, o->width * o->height * BPP);
  o->buffer = NULL;
}

void create_layer_surface(struct output *o) {
  int wl_surface_id;
  int wlr_layer_surface_id;

  wl_surface_id = xdwl_object_register(proxy, 0, "wl_surface");
  if (wl_surface_id == -1)
    xdwlw_exit();

  wlr_layer_surface_id =
      xdwl_object_register(proxy, 0, "zwlr_layer_surface_v1");
  if (wlr_layer_surface_id == -1)
    xdwlw_exit();

  if (xdwl_compositor_create_surface(proxy, wl_surface_id) == -1)
    xdwlw_exit();

  if (xdzwlr_layer_shell_v1_get_layer_surface(
          proxy, wlr_layer_surface_id, wl_surface_id, o->id,
          XDZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, o->name) == -1)
    xdwlw_exit();

  xdzwlr_layer_surface_v1_set_size(proxy, o->logical_width, o->logical_height);

  xdzwlr_layer_surface_v1_set_anchor(proxy, XDZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
  xdzwlr_layer_surface_v1_set_anchor(proxy,
                                     XDZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

  struct xdzwlr_layer_surface_v1_event_handlers *zwlr_layer_surface_v1 =
      xdwl_list_push(global_listeners,
                     &(struct xdzwlr_layer_surface_v1_event_handlers){
                         .configure = handle_zwlr_layer_surface_v1_configure},
                     sizeof(struct xdzwlr_layer_surface_v1_event_handlers));

  ensure_result(
      xdzwlr_layer_surface_v1_add_listener(proxy, zwlr_layer_surface_v1, o));

  xdwl_surface_commit(proxy);
  ensure_result(xdwl_roundtrip(proxy));
}

void setup_output(struct output *o) {
  if (o->busy == 0) {
    create_layer_surface(o);
    o->busy = 1;
  }
}

int main(int argc, char **argv) {
  xdwl_list *l;
  struct output *o;
  struct ipc_message *msg;

  int cfd = 0;
  sfd = ipc_server_start();

  if (sfd < 0) {
    xdwlw_error_set(XDWLWE_DSTART,
                    "xdwlwd: failed to start (failed to open socket)");
    xdwlw_exit();
  }

  xdwl_list *outputs = init();
  if (!outputs) {
    xdwlw_exit();
  }

  xdwl_list_for_each(l, outputs, o) {
    xdwlw_log("info", "found %s output. res: %dx%d", o->name, o->width,
              o->height, o->logical_width, o->logical_height);
  }
  struct ipc_message reply;
  while (1) {
    msg = ipc_server_listen(sfd, &cfd);
    if (msg != NULL) {
      switch (msg->type) {
      case IPC_CLIENT_SET_COLOR:
        xdwl_list_for_each(l, outputs, o) {
          if (STREQ(o->name, msg->set_color.output) ||
              STREQ(msg->set_color.output, "all")) {

            o->color = msg->set_color.color;
            setup_output(o);

            if (draw_color_on_output(o, msg->set_color.color) != 0) {
              reply.type = IPC_SERVER_ERR;
              snprintf(reply.error.msg, sizeof(reply.error.msg), "%s",
                       xdwlw_error_get_msg());
              ipc_server_send(cfd, &reply);
            }
          } else {
            reply.type = IPC_SERVER_ERR;
            snprintf(reply.error.msg, sizeof(reply.error.msg),
                     "unknown output: %s", msg->set_color.output);

            ipc_server_send(cfd, &reply);
          }
        }
        ipc_server_send(cfd, &(struct ipc_message){.type = IPC_ACK});
        break;

      case IPC_CLIENT_SET_IMAGE:
        xdwl_list_for_each(l, outputs, o) {
          if (STREQ(o->name, msg->set_image.output) ||
              STREQ(msg->set_image.output, "all")) {

            o->image_path = msg->set_image.path;
            o->image_mode = msg->set_image.mode;

            setup_output(o);

            if (draw_image_on_output(o) != 0) {
              reply.type = IPC_SERVER_ERR;
              snprintf(reply.error.msg, sizeof(reply.error.msg), "%s",
                       xdwlw_error_get_msg());
              ipc_server_send(cfd, &reply);
            }
          } else {
            reply.type = IPC_SERVER_ERR;
            snprintf(reply.error.msg, sizeof(reply.error.msg),
                     "unknown output: %s", msg->set_image.output);

            ipc_server_send(cfd, &reply);
          }
        }
        ipc_server_send(cfd, &(struct ipc_message){.type = IPC_ACK});
        break;

      case IPC_CLIENT_KILL:
        xdwlw_log("info", "killed. exiting");
        ipc_server_send(cfd, &(struct ipc_message){.type = IPC_ACK});
        goto exit;

      default:
        xdwlw_log("warn", "unknown message");
        break;
      }
    }
  }

exit:
  xdwl_list_for_each(l, outputs, o) { free(o->name); }
  xdwl_list_destroy(outputs);
  xdwlw_exit();
}
