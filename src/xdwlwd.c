#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-types.h"
#include <stddef.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "xdwlw-common.h"
#include "xdwlw-error.h"
#include "xdwlw-ipc.h"
#include "xdwlw-outputs.h"
#include "xdwlw-types.h"

#include <dirent.h>
#include <linux/limits.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#define LERP(v0, v1, t) ((1 - t) * v0 + t * v1);
#define COLOR(hex)                                                             \
  {((hex >> 24) & 0xFF), ((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF),            \
   (hex & 0xFF)}

#define ENSURE_RESULT(n)                                                       \
  if ((n) == -1)                                                               \
  xdwlw_exit()

enum wallpaper_type { IMAGE = 0x10, COLOR = 0x20 };

int sfd;
xdwl_proxy *proxy;
xdwl_list *global_listeners;

void xdwlw_exit();
void apply(struct output *o);

void handle_wl_registry_global(void *globals, xdwl_arg *args);
void handle_wl_display_error(void *_, xdwl_arg *args);
void handle_wl_display_delete_id(void *_, xdwl_arg *args);
void handle_wl_output_mode(void *outputs, xdwl_arg *args);
void handle_xdg_output_logical_size(void *output, xdwl_arg *args);
void handle_xdg_output_name(void *output, xdwl_arg *args);
void handle_zwlr_layer_surface_v1_configure(void *output, xdwl_arg *args);

static const char *get_cache_path() {
  char usr_cache_path[PATH_MAX - 12];
  static char cache_path[PATH_MAX];
  const char *home = getenv("HOME");

  snprintf(usr_cache_path, sizeof(usr_cache_path), "%s/.cache", home);
  if (access(usr_cache_path, R_OK) != 0)
    mkdir(usr_cache_path, 0750);

  snprintf(cache_path, sizeof(cache_path), "%s/xdwlw", usr_cache_path);
  if (access(cache_path, R_OK) != 0)
    mkdir(cache_path, 0750);

  return cache_path;
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

  apply(o);
  xdwlw_log("info", "set %s (mode %c) on %s", o->image_path, o->image_mode,
            o->name);

  return 0;
};

int draw_color_on_output(struct output *o) {
  for (size_t i = 0; i < o->width * o->height; i++) {
    o->buffer[i] = o->color;
  }

  int color_bytes[] = COLOR(o->color);
  apply(o);
  xdwlw_log("info", "set #%02x%02x%02x on %s", color_bytes[1], color_bytes[2],
            color_bytes[3], o->name);
  return 0;
};

void apply(struct output *o) {
  xdwl_object *wl_buffer_object = xdwl_object_get_by_name(proxy, "wl_buffer");
  if (!wl_buffer_object) {
    xdwlw_exit();
  }

  uint32_t wl_buffer_id = wl_buffer_object->id;
  xdwl_surface_attach(proxy, wl_buffer_id, 0, 0);
  xdwl_surface_damage_buffer(proxy, 0, 0, o->width * BPP, o->height);
  xdwl_surface_commit(proxy);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
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

  ENSURE_RESULT(
      xdzwlr_layer_surface_v1_add_listener(proxy, zwlr_layer_surface_v1, o));

  xdwl_surface_commit(proxy);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
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

  ENSURE_RESULT(xdwl_registry_add_listener(proxy, wl_registry, *globals));

  xdwl_display_get_registry(proxy, wl_registry_id);
  ENSURE_RESULT(xdwl_roundtrip(proxy));

  size_t n = WP_VIEWPORTER | ZWLR_LAYER_SHELL_V1;

  size_t i;
  xdwl_list *l;
  for (l = *globals, i = 0; l->next; i++, l = l->next) {
    struct wl_global *g = l->data;

    int new_id = 0;
    if (STREQ(g->interface, "wl_shm") || STREQ(g->interface, "wl_compositor")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      if (new_id == -1) {
        xdwlw_error_set(XDWLWE_NOIFACE, "failed to register %s", g->interface);
        xdwlw_exit();
      }

    } else if (STREQ(g->interface, "wp_viewporter")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      if (new_id == -1) {
        xdwlw_error_set(XDWLWE_NOIFACE, "failed to register %s", g->interface);
        xdwlw_exit();
      }

      n &= ~WP_VIEWPORTER;

    } else if (STREQ(g->interface, "zwlr_layer_shell_v1")) {
      new_id = xdwl_object_register(proxy, 0, g->interface);
      n &= ~ZWLR_LAYER_SHELL_V1;
    }

    if (new_id > 0) {
      xdwl_registry_bind(proxy, g->name, g->interface, g->version, new_id);
      ENSURE_RESULT(xdwl_roundtrip(proxy));
      xdwl_list_remove(globals, i);
    }
  }

  return n;
};

xdwl_list *xdwlw_init() {
  global_listeners = xdwl_list_new();
  if (!global_listeners)
    return NULL;

  proxy = xdwl_proxy_create();
  if (!proxy)
    return NULL;

  ENSURE_RESULT(xdwl_object_register(proxy, 0, "wl_display"));
  struct xdwl_display_event_handlers *wl_display =
      xdwl_list_push(global_listeners,
                     &(struct xdwl_display_event_handlers){
                         .delete_id = handle_wl_display_delete_id,
                         .error = handle_wl_display_error},
                     sizeof(struct xdwl_display_event_handlers));

  ENSURE_RESULT(xdwl_display_add_listener(proxy, wl_display, NULL));

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

void setup_output(struct output *o) {
  if (o->busy == 0) {
    create_layer_surface(o);
    o->busy = 1;
  }
}

void read_from_cache(struct output *o) {
  const char *cache_path = get_cache_path();
  DIR *dirp = opendir(cache_path);

  struct dirent *dp;

  do {
    if ((dp = readdir(dirp)) != NULL && strcmp(o->name, dp->d_name) == 0) {
      char filepath[PATH_MAX];
      snprintf(filepath, sizeof(filepath), "%s/%s", cache_path, dp->d_name);

      FILE *f = fopen(filepath, "rb");
      if (f == NULL) {
        xdwlw_error_set(XDWLWE_NOFILE, "failed to open %s for reading",
                        filepath);
        xdwlw_exit();
      }

      enum wallpaper_type type = fgetc(f);

      size_t offset = 0;
      size_t buffer_size = 0;

      switch (type) {
      case COLOR:
        buffer_size = 4;
        break;

      case IMAGE:
        buffer_size = 1 + 2 + PATH_MAX;
        break;

      default:
        fclose(f);
        return;
      }

      uint8_t buffer[buffer_size];
      fread(buffer, 1, buffer_size, f);

      switch (type) {
      case COLOR:
        o->color = buf_read_u32(buffer, &offset);
        break;

      case IMAGE:
        o->image_mode = buf_read_u8(buffer, &offset);
        o->image_path = strdup(buf_read_string(buffer, &offset));
        break;
      }

      fclose(f);
      return;
    }
  } while (dp != NULL);
};

void write_to_cache(struct output *o) {
  const char *cache_path = get_cache_path();
  char filepath[PATH_MAX];
  snprintf(filepath, sizeof(filepath), "%s/%s", cache_path, o->name);

  size_t offset = 0;

  FILE *f = fopen(filepath, "wb");
  if (f == NULL) {
    xdwlw_error_set(XDWLWE_NOFILE, "failed to open %s for writing", filepath);
    xdwlw_exit();
  }

  size_t buffer_size;
  if (o->image_path != NULL && o->image_mode != 0) {
    // type + image mode + string length + image path
    buffer_size = 1 + 1 + 2 + strlen(o->image_path);

  } else {
    // type + color
    buffer_size = 1 + 4;
  }

  char buffer[buffer_size];
  if (o->image_path != NULL && o->image_mode != 0) {
    buf_write_u8(buffer, &offset, IMAGE);
    buf_write_u8(buffer, &offset, o->image_mode);
    buf_write_string(buffer, &offset, o->image_path);

  } else {
    buf_write_u8(buffer, &offset, COLOR);
    buf_write_u32(buffer, &offset, (uint32_t)o->color);
  }

  fwrite(buffer, 1, sizeof(buffer), f);
  fclose(f);
};

int main() {
  xdwl_list *l;
  struct output *o;
  struct ipc_message *msg;
  struct ipc_message reply;

  int cfd = 0;
  sfd = ipc_server_start();

  if (sfd < 0) {
    xdwlw_error_set(XDWLWE_DSTART,
                    "xdwlwd: failed to start (failed to open socket)");
    xdwlw_exit();
  }

  xdwl_list *outputs = xdwlw_init();
  if (!outputs) {
    xdwlw_exit();
  }

  xdwl_list_for_each(l, outputs, o) {
    xdwlw_log("info", "found output: %s %dx%d", o->name, o->width, o->height,
              o->logical_width, o->logical_height);

    setup_output(o);

    o->color = -1;
    o->image_path = NULL;
    o->image_mode = 0;

    read_from_cache(o);

    if (o->image_path != NULL && o->image_mode != 0) {
      if (draw_image_on_output(o) != 0)
        xdwlw_exit();

    } else if (o->color != -1) {
      if (draw_color_on_output(o) != 0)
        xdwlw_exit();
    }
  }

  while (1) {
    msg = ipc_server_listen(sfd, &cfd);
    if (msg != NULL) {
      switch (msg->type) {
      case IPC_CLIENT_SET_COLOR:
        xdwl_list_for_each(l, outputs, o) {
          if (STREQ(o->name, msg->set_color.output) ||
              STREQ(msg->set_color.output, "all")) {

            o->color = msg->set_color.color;
            if (o->image_path != NULL)
              free((char *)o->image_path);
            o->image_path = NULL;
            o->image_mode = 0;

            setup_output(o);
            write_to_cache(o);
            if (draw_color_on_output(o) != 0) {
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

            o->color = -1;

            if (o->image_path != NULL)
              free((char *)o->image_path);

            o->image_path = msg->set_image.path;
            o->image_mode = msg->set_image.mode;

            setup_output(o);
            write_to_cache(o);

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
  xdwl_list_for_each(l, outputs, o) {
    free(o->name);
    if (o->image_path != NULL)
      free((char *)o->image_path);
  }
  xdwl_list_destroy(outputs);
  xdwlw_exit();
}
