#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-types.h"
#include <stdint.h>
#include <time.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "../xdg-output-unstable-v1-protocol.h"
#include "xdwlw-common.h"
#include "xdwlw-error.h"
#include "xdwlw-ipc.h"
#include "xdwlw-types.h"

#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <sys/stat.h>

#define LERP(v0, v1, t) ((1 - t) * v0 + t * v1);
#define COLOR(hex)                                                             \
  {((hex >> 24) & 0xFF), ((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF),            \
   (hex & 0xFF)}

enum wallpaper_type { IMAGE = 0x10, COLOR = 0x20 };

int sfd;
xdwl_proxy *proxy;
xdwl_list *outputs = NULL;

void xdwlw_init();
void setup_globals();
void xdwlw_exit(int);

void read_from_cache(struct output *);
void write_to_cache(struct output *);

void create_layer_surface(struct output *);
void apply(struct output *);
void *create_wl_buffer(uint32_t, uint32_t, uint32_t *);
void destroy_output_buffer(struct output *);

void handle_wl_display_error(void *, xdwl_arg *);
void handle_wl_display_delete_id(void *, xdwl_arg *);
void handle_wl_registry_global(void *, xdwl_arg *);
void handle_wl_output_mode(void *, xdwl_arg *);
void handle_wl_output_name(void *, xdwl_arg *);
void handle_wl_pointer_enter(void *, xdwl_arg *);
void handle_xdg_output_logical_size(void *, xdwl_arg *);
void handle_zwlr_layer_surface_v1_closed(void *, xdwl_arg *);
void handle_zwlr_layer_surface_v1_configure(void *, xdwl_arg *);

uint32_t *resize_image_nni(uint32_t *i_buffer, size_t i_width, size_t i_height,
                           size_t o_width, size_t o_height);

int draw_image_on_output(struct output *o);
int draw_color_on_output(struct output *o);
void apply_saved_wallpapers(struct output *o);

void handle_wl_output_mode(void *_, xdwl_arg *args) {
  xdwl_list *l;
  struct output *o;
  xdwl_list_for_each(l, outputs, o) {
    if (o->id == args[0].object_id) {
      o->width = args[2].i;
      o->height = args[3].i;
    }
  }
};

void handle_wl_output_name(void *_, xdwl_arg *args) {
  struct output *new_output = NULL;
  uint32_t new_output_id = args[0].object_id;
  const char *new_output_name = args[1].s;

  struct output *old_output = NULL;
  size_t old_output_idx;

  xdwl_list *l;
  struct output *o;

  size_t i = 0;

  xdwl_list_for_each(l, outputs, o) {
    uint32_t output_id = o->id;
    const char *output_name = o->name;
    if (o->name != NULL && STREQ(output_name, new_output_name)) {
      old_output = o;
      old_output_idx = i;

    } else if (output_id == new_output_id) {
      new_output = o;
    }
    i++;
  }

  if (!new_output) {
    xdwlw_error_set(XDWLWE_NOOUPUT, "no wl_output#%ld found",
                    args[0].object_id);
    xdwlw_exit(0);
  }

  if (old_output != NULL) {
    new_output->id = new_output_id;
    new_output->name = old_output->name;
    new_output->color = old_output->color;
    new_output->image_path = old_output->image_path;
    new_output->image_mode = old_output->image_mode;
    new_output->buffer = old_output->buffer;
    new_output->surface_id = old_output->surface_id;
    xdwl_list_remove(&outputs, old_output_idx);

  } else {
    new_output->name = strdup(new_output_name);
  }
}

void handle_wl_pointer_enter(void *cursor_surface, xdwl_arg *args) {
  uint32_t serial = args[1].u;
  float surface_x = args[3].f;
  float surface_y = args[4].f;

  xdwl_pointer_set_cursor(proxy, 0, serial, *(uint32_t *)cursor_surface,
                          surface_x, surface_y);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
}

void handle_xdg_output_logical_size(void *output, xdwl_arg *args) {
  ((struct output *)output)->logical_width = args[1].i;
  ((struct output *)output)->logical_height = args[2].i;
};

void get_output_logical_size(struct output *o) {
  struct xdzxdg_output_v1_event_handlers zxdg_output_v1 = {
      .logical_size = handle_xdg_output_logical_size,
  };

  uint32_t zxdg_output_id = xdwl_object_register(proxy, 0, "zxdg_output_v1");
  ENSURE_RESULT(xdzxdg_output_v1_add_listener(proxy, &zxdg_output_v1, o));

  xdzxdg_output_manager_v1_get_xdg_output(proxy, 0, zxdg_output_id, o->id);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
}

void *create_wl_buffer(uint32_t width, uint32_t height,
                       uint32_t *wl_buffer_id) {
  size_t wl_shm_pool_id;

  char name[255];
  snprintf(name, 255, "xdwlw-shm-%x", rand());

  int32_t buffer_size = width * height * BPP;

  int fd = shm_open(name, O_RDWR | O_EXCL | O_CREAT, 0600);
  shm_unlink(name);

  if (fd == -1) {
    perror("shm_open");
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create %s output buffer",
                    name);
    xdwlw_exit(0);
  }

  if (ftruncate(fd, buffer_size) == -1) {
    perror("ftruncate");
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create %s output buffer",
                    name);
    xdwlw_exit(0);
  }

  xdwl_object *wl_shm_pool_object =
      xdwl_object_get_by_name(proxy, "wl_shm_pool");

  if (wl_shm_pool_object == NULL) {
    wl_shm_pool_id = xdwl_object_register(proxy, 0, "wl_shm_pool");
    xdwl_shm_create_pool(proxy, 0, wl_shm_pool_id, fd, buffer_size);
  } else {
    wl_shm_pool_id = wl_shm_pool_object->id;
  }

  *wl_buffer_id = xdwl_object_register(proxy, 0, "wl_buffer");
  xdwl_shm_pool_create_buffer(proxy, 0, *wl_buffer_id, 0, width, height,
                              width * BPP, XDWL_SHM_FORMAT_ARGB8888);

  uint32_t *buffer =
      mmap(NULL, buffer_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (buffer == MAP_FAILED) {
    perror("mmap");
    xdwlw_error_set(XDWLWE_NOOUPUTBUF, "failed to create %s output buffer",
                    name);
    xdwlw_exit(0);
  }

  xdwlw_log("info", "initialized buffer %s with a size of %d", name,
            buffer_size);

  return buffer;
}

void destroy_output_buffer(struct output *o) {
  if (!o)
    return;

  munmap(o->buffer, o->width * o->height * BPP);
  o->buffer = NULL;
}

void handle_zwlr_layer_surface_v1_closed(void *output, xdwl_arg *args) {
  ENSURE_RESULT(xdzwlr_layer_surface_v1_destroy(proxy, args[0].object_id));
  ENSURE_RESULT(
      xdwl_surface_destroy(proxy, ((struct output *)output)->surface_id));

  xdwl_destroy_listener(args[0].object_id);
  xdwlw_log("info", "wlr_layer_surface#%ld was destroyed", args[0].object_id);
}

void handle_zwlr_layer_surface_v1_configure(void *output, xdwl_arg *args) {
  struct output *o = output;

  uint32_t serial = args[1].u;
  uint32_t width = args[2].u;
  uint32_t height = args[3].u;

  xdzwlr_layer_surface_v1_ack_configure(proxy, 0, serial);

  if (o->width != width || o->height != height) {
    destroy_output_buffer(o);

    o->width = width;
    o->height = height;
  }

  if (o->buffer == NULL) {
    o->buffer = create_wl_buffer(o->width, o->height, &o->buffer_id);
  }
}

void create_surface(struct output *o) {
  int wl_surface_id = xdwl_object_register(proxy, 0, "wl_surface");
  if (wl_surface_id == -1)
    xdwlw_exit(0);

  if (xdwl_compositor_create_surface(proxy, 0, wl_surface_id) == -1)
    xdwlw_exit(0);

  o->surface_id = wl_surface_id;
}

void create_layer_surface(struct output *o) {
  int wlr_layer_surface_id =
      xdwl_object_register(proxy, 0, "zwlr_layer_surface_v1");
  if (wlr_layer_surface_id == -1)
    xdwlw_exit(0);

  if (xdzwlr_layer_shell_v1_get_layer_surface(
          proxy, 0, wlr_layer_surface_id, o->surface_id, o->id,
          XDZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, o->name) == -1)
    xdwlw_exit(0);

  xdzwlr_layer_surface_v1_set_size(proxy, 0, o->logical_width,
                                   o->logical_height);

  xdzwlr_layer_surface_v1_set_anchor(proxy, 0,
                                     XDZWLR_LAYER_SURFACE_V1_ANCHOR_TOP);
  xdzwlr_layer_surface_v1_set_anchor(proxy, 0,
                                     XDZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);

  struct xdzwlr_layer_surface_v1_event_handlers zwlr_layer_surface_v1 = {
      .configure = handle_zwlr_layer_surface_v1_configure,
      .closed = handle_zwlr_layer_surface_v1_closed};

  ENSURE_RESULT(
      xdzwlr_layer_surface_v1_add_listener(proxy, &zwlr_layer_surface_v1, o));

  xdwl_surface_commit(proxy, 0);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
}

void handle_wl_registry_global(void *_, xdwl_arg *args) {
  uint32_t name = args[1].u;
  const char *interface = args[2].s;
  uint32_t version = args[3].u;

  int new_id = 0;

  if (STREQ(interface, "wl_shm") || STREQ(interface, "wl_compositor") ||
      STREQ(interface, "wp_viewporter") ||
      STREQ(interface, "zwlr_layer_shell_v1") ||
      STREQ(interface, "zxdg_output_manager_v1")) {

    new_id = xdwl_object_register(proxy, 0, interface);
    if (new_id == -1) {
      xdwlw_error_set(XDWLWE_NOIFACE, "failed to register %s", interface);
      xdwlw_exit(0);
    }

    xdwl_registry_bind(proxy, 0, name, interface, version, new_id);
    ENSURE_RESULT(xdwl_roundtrip(proxy));
  } else if (STREQ(interface, "wl_output")) {
    new_id = xdwl_object_register(proxy, 0, interface);
    if (new_id == -1) {
      xdwlw_error_set(XDWLWE_NOIFACE, "failed to register %s", interface);
      xdwlw_exit(0);
    }

    struct xdwl_output_event_handlers wl_output_event_handers = {
        .mode = handle_wl_output_mode,
        .name = handle_wl_output_name,
    };
    ENSURE_RESULT(
        xdwl_output_add_listener(proxy, &wl_output_event_handers, NULL));

    struct output *o;
    if ((o = xdwl_list_push(outputs,
                            &(struct output){
                                .id = new_id,
                                .buffer = NULL,
                                .color = -1,
                                .image_mode = 0,
                                .image_path = NULL,
                            },
                            sizeof(struct output))) == NULL) {
      xdwlw_exit(0);
    };

    xdwl_registry_bind(proxy, 0, name, interface, version, new_id);
    ENSURE_RESULT(xdwl_roundtrip(proxy));
    get_output_logical_size(o);

    xdwlw_log("info", "found output: %s %dx%d (%dx%d)", o->name, o->width,
              o->height, o->logical_width, o->logical_height);

    create_surface(o);
    create_layer_surface(o);
    apply_saved_wallpapers(o);
  }
}

void handle_wl_display_error(void *_, xdwl_arg *args) {
  uint32_t object_id = args[1].u;
  uint32_t code = args[2].u;
  const char *message = args[3].s;

  const xdwl_object *object = xdwl_object_get_by_id(proxy, object_id);
  const char *object_name = object->name;

  printf("%s#%d ERROR %d: %s\n", object_name, object_id, code, message);
}

void handle_wl_display_delete_id(void *_, xdwl_arg *args) {
  size_t object_id = args[1].u;
  assert(xdwl_object_unregister(proxy, object_id) == 0);
};

const char *get_cache_path() {
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

void apply_saved_wallpapers(struct output *o) {
  if (o->image_path != NULL && o->image_mode != 0) {
    if (draw_image_on_output(o) != 0)
      xdwlw_exit(0);

  } else if (o->color != -1) {
    if (draw_color_on_output(o) != 0)
      xdwlw_exit(0);
  }
}

void apply(struct output *o) {
  xdwl_surface_attach(proxy, 0, o->buffer_id, 0, 0);
  xdwl_surface_damage_buffer(proxy, 0, 0, 0, o->width * BPP, o->height);
  xdwl_surface_commit(proxy, 0);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
}

void setup_globals() {
  size_t wl_registry_id = xdwl_object_register(proxy, 0, "wl_registry");

  struct xdwl_registry_event_handlers wl_registry_event_handlers = {
      .global = handle_wl_registry_global};

  ENSURE_RESULT(
      xdwl_registry_add_listener(proxy, &wl_registry_event_handlers, NULL));

  xdwl_display_get_registry(proxy, wl_registry_id);
  ENSURE_RESULT(xdwl_roundtrip(proxy));
};

void xdwlw_init() {
  proxy = xdwl_proxy_create();
  if (!proxy)
    xdwlw_exit(0);

  if (xdwl_object_register(proxy, 0, "wl_display") == 0)
    xdwlw_exit(0);

  struct xdwl_display_event_handlers wl_display = {
      .delete_id = handle_wl_display_delete_id,
      .error = handle_wl_display_error};

  ENSURE_RESULT(xdwl_display_add_listener(proxy, &wl_display, NULL));

  outputs = xdwl_list_new();
  if (outputs == NULL)
    xdwlw_exit(0);

  setup_globals();
}

void xdwlw_exit(int _) {
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

  printf("\n");
  exit(xdwl_err || xdwlw_err);
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
        xdwlw_exit(0);
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
    xdwlw_exit(0);
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

void *wayland_listen(void *_) {
  while (xdwl_dispatch(proxy) != -1)
    ;

  xdwlw_exit(0);
  return NULL;
}

int main() {
  xdwl_list *l;
  struct output *o;
  struct ipc_message *msg;
  struct ipc_message reply;
  pthread_t thread;

  signal(SIGTERM, xdwlw_exit);
  signal(SIGINT, xdwlw_exit);

  int cfd = 0;
  sfd = ipc_server_start();

  if (sfd < 0) {
    xdwlw_error_set(XDWLWE_DSTART,
                    "xdwlwd: failed to start (failed to open socket)");
    xdwlw_exit(0);
  }

  xdwlw_init();
  if (!outputs) {
    xdwlw_exit(0);
  }

  xdwl_list_for_each(l, outputs, o) {
    o->color = -1;
    o->image_path = NULL;
    o->image_mode = 0;

    read_from_cache(o);
    apply_saved_wallpapers(o);
  }

  pthread_create(&thread, NULL, &wayland_listen, NULL);
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
  xdwlw_exit(0);
}
