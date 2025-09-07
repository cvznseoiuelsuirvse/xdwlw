#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"

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

void handle_zwlr_layer_surface_v1_configure(void *data, xdwl_arg *args);
void handle_wl_display_delete_id(void *data, xdwl_arg *args);
void handle_wl_display_error(void *data, xdwl_arg *args);
void handle_wl_registry_global(void *globals, xdwl_arg *args);
void handle_wl_buffer_release(void *globals, xdwl_arg *args);

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

  struct xdwl_registry *wl_registry = xdwl_list_push(
      global_listeners,
      &(struct xdwl_registry){.global = handle_wl_registry_global},
      sizeof(struct xdwl_registry));
  ensure_result(xdwl_registry_add_listener(proxy, wl_registry, *globals));

  xdwl_display_get_registry(proxy, wl_registry_id);
  ensure_result(xdwl_roundtrip(proxy));

  size_t n = WP_VIEWPORTER | ZWLR_LAYER_SHELL_V1;

  size_t i;
  xdwl_list *l;
  for (l = *globals, i = 0; l->next; i++, l = l->next) {
    struct wl_global *g = l->data;

    size_t new_id = 0;
    if (STREQ(g->interface, "wl_shm") || STREQ(g->interface, "wl_compositor")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);

    } else if (STREQ(g->interface, "wp_viewporter")) {
      ensure_result(xdwl_load_interfaces(
          "/usr/share/wayland-protocols/stable/viewporter/viewporter.xml"));
      new_id = xdwl_object_register(proxy, 0, g->interface);
      n &= ~WP_VIEWPORTER;

    } else if (STREQ(g->interface, "zwlr_layer_shell_v1")) {
      ensure_result(
          xdwl_load_interfaces("./protocols/wlr-layer-shell-unstable-v1.xml"));
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

void init_buffer(struct output *o) {
  char name[255];
  snprintf(name, 255, "xdwlw-shm-%x", rand());

  int32_t buffer_size = o->width * o->height * BPP;

  int fd = shm_open(name, O_RDWR | O_EXCL | O_CREAT, 0600);
  shm_unlink(name);

  if (fd == -1) {
    perror("shm_open");
    return;
  }

  if (ftruncate(fd, buffer_size) == -1) {
    perror("ftruncate");
    return;
  }

  struct xdwl_object *wl_shm_pool_object;
  size_t wl_shm_pool_id;

  wl_shm_pool_object = xdwl_object_get_by_name(proxy, "wl_shm_pool");
  if (wl_shm_pool_object != NULL) {
    xdwl_shm_pool_destroy(proxy);
    ensure_result(xdwl_roundtrip(proxy));
  }

  wl_shm_pool_id = xdwl_object_register(proxy, 0, "wl_shm_pool");
  xdwl_shm_create_pool(proxy, wl_shm_pool_id, fd, buffer_size);
  ensure_result(xdwl_roundtrip(proxy));

  size_t wl_buffer_id = xdwl_object_register(proxy, 0, "wl_buffer");
  struct xdwl_buffer *wl_buffer =
      xdwl_list_push(global_listeners,
                     &(struct xdwl_buffer){.release = handle_wl_buffer_release},
                     sizeof(struct xdwl_buffer));
  ensure_result(xdwl_buffer_add_listener(proxy, wl_buffer, o));

  xdwl_shm_pool_create_buffer(proxy, wl_buffer_id, 0, o->width, o->height,
                              o->width * BPP, XDWL_SHM_FORMAT_ARGB8888);
  ensure_result(xdwl_roundtrip(proxy));

  uint32_t *buffer =
      mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (buffer == MAP_FAILED) {
    perror("mmap");
    return;
  }

  xdwlw_log("info", "initialized buffer %s with a size of %d", name,
            buffer_size);

  xdwlw_log("debug", "fd: %d", fd);
  o->buffer = buffer;
  o->fd = fd;
}

void create_layer_surface(struct output *o) {
  size_t wl_surface_id = xdwl_object_register(proxy, 0, "wl_surface");
  size_t zwlr_layer_surface_v1_id =
      xdwl_object_register(proxy, 0, "zwlr_layer_surface_v1");

  struct xdzwlr_layer_surface_v1 *i =
      xdwl_list_push(global_listeners,
                     &(struct xdzwlr_layer_surface_v1){
                         .configure = handle_zwlr_layer_surface_v1_configure},
                     sizeof(struct xdzwlr_layer_surface_v1));
  ensure_result(xdzwlr_layer_surface_v1_add_listener(proxy, i, NULL));

  xdwl_compositor_create_surface(proxy, wl_surface_id);
  xdzwlr_layer_shell_v1_get_layer_surface(
      proxy, zwlr_layer_surface_v1_id, wl_surface_id, o->id,
      XDZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, o->name);

  xdwlw_log("info", "created layer surface#%d for %s", zwlr_layer_surface_v1_id,
            o->name);
}

void configure_layer_surface(struct output *o) {
  xdzwlr_layer_surface_v1_set_anchor(proxy, XDZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
  xdzwlr_layer_surface_v1_set_anchor(proxy,
                                     XDZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

  xdzwlr_layer_surface_v1_set_size(proxy, o->logical_width, o->logical_height);
  xdzwlr_layer_surface_v1_set_exclusive_zone(proxy, -1);

  xdwlw_log("info", "set %s layer surface size to: %dx%d", o->name,
            o->logical_width, o->logical_height);

  xdwl_surface_commit(proxy);
  ensure_result(xdwl_roundtrip(proxy));
};

int setup_output(struct output *o) {
  if (o->fd != 0 && o->buffer != NULL)
    return 0;

  if (o->busy == 0) {
    create_layer_surface(o);
    configure_layer_surface(o);
    o->busy = 1;
  }

  init_buffer(o);
  if (o->buffer == NULL) {
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create buffer for %s output",
                    o->name);
    return -1;
  }

  return 0;
}

void attach_and_commit(struct output *o) {
  xdwlw_log("debug", "buffer ptr: %p", o->buffer);
  xdwl_object *wl_buffer = xdwl_object_get_by_name(proxy, "wl_buffer");
  size_t wl_buffer_id = wl_buffer->id;

  xdwl_surface_attach(proxy, wl_buffer_id, 0, 0);
  xdwl_surface_commit(proxy);

  ensure_result(xdwl_roundtrip(proxy));
}

int draw_color_on_output(struct output *o, uint32_t color) {
  int r = setup_output(o);
  if (r != 0)
    return 1;

  for (size_t i = 0; i < o->width * o->height; i++) {
    o->buffer[i] = color;
  }

  attach_and_commit(o);

  int color_bytes[] = COLOR(color);
  xdwlw_log("info", "set new buffer color to #%02x%02x%02x%02x", color_bytes[1],
            color_bytes[2], color_bytes[3], color_bytes[0]);

  return 0;
};

int draw_image_on_output(struct output *o, const char *image_path,
                         enum image_modes mode) {
  int r = setup_output(o);
  if (r != 0)
    return 1;

  int iw, ih, channels;

  uint32_t *pixels = (uint32_t *)stbi_load(image_path, &iw, &ih, &channels, 4);

  if (!pixels) {
    xdwlw_error_set(XDWLWE_IMGOPEN, "failed to open %s", image_path);
    return 1;
  }
  xdwlw_log("info", "loaded %s: %dx%d channels: %d", image_path, iw, ih,
            channels);

  float sx, sy, scale;
  size_t nw, nh, left, top, right, bottom;

  uint32_t *resized;

  switch (mode) {
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
                      "failed to resize %s from %dx%d to %dx%d", image_path, iw,
                      ih, o->width, o->height);

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
                      "failed to resize %s from %dx%d to %dx%d", image_path, iw,
                      ih, o->width, o->height);

      stbi_image_free(pixels);
      return 1;
    }

    memcpy(o->buffer, resized, o->width * o->height * BPP);
    break;
  default:
    xdwlw_error_set(XDWLWE_IMGINVMODE, "invalid image mode: %c", mode);
    return 1;
  }

  free(resized);
  stbi_image_free(pixels);

  attach_and_commit(o);
  xdwlw_log("info", "set %s on %s. mode: %c", image_path, o->name, mode);

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
  struct xdwl_display *wl_display = xdwl_list_push(
      global_listeners,
      &(struct xdwl_display){.delete_id = handle_wl_display_delete_id,
                             .error = handle_wl_display_error},
      sizeof(struct xdwl_display));

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

int main(int argc, char **argv) {
  xdwl_list *l;
  struct output *o;
  struct ipc_message *msg;

  xdwl_list *outputs = init();
  if (!outputs) {
    xdwlw_exit();
  }

  xdwl_list_for_each(l, outputs, o) {
    xdwlw_log("info", "found %s output. res: %dx%d", o->name, o->width,
              o->height, o->logical_width, o->logical_height);
  }

  int cfd = 0;
  sfd = ipc_server_start();

  if (sfd < 0) {
    xdwlw_error_set(XDWLWE_DSTART,
                    "xdwlwd: failed to start (failed to open socket)");
    xdwl_list_for_each(l, outputs, o) { free(o->name); }

    xdwl_list_destroy(outputs);
    xdwlw_exit();
  }

  while (1) {
    msg = ipc_server_listen(sfd, &cfd);
    if (msg != NULL) {
      switch (msg->type) {
      case IPC_CLIENT_SET_COLOR:
        xdwl_list_for_each(l, outputs, o) {
          if (draw_color_on_output(o, msg->set_color.color) != 0) {
            struct ipc_message reply = {.type = IPC_SERVER_ERR};
            reply.error.msg = xdwlw_error_get_msg();

            ipc_server_send(cfd, &reply);
          }
        }
        ipc_server_send(cfd, &(struct ipc_message){.type = IPC_ACK});
        break;

      case IPC_CLIENT_SET_IMAGE:
        xdwl_list_for_each(l, outputs, o) {
          if (draw_image_on_output(o, msg->set_image.path,
                                   msg->set_image.mode) != 0) {
            struct ipc_message reply = {.type = IPC_SERVER_ERR};
            reply.error.msg = xdwlw_error_get_msg();

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
