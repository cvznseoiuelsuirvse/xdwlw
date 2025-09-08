#include "xdwayland-core.h"

#include "../wlr-layer-shell-unstable-v1-protocol.h"
#include "xdwlw-common.h"
#include "xdwlw-error.h"
#include "xdwlw-types.h"

#include <fcntl.h>

void xdwlw_exit();

extern xdwl_map *global_listeners;
extern xdwl_proxy *proxy;

static void create_output_buffer(struct output *o) {
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

static void destroy_output_buffer(struct output *o) {
  if (!o)
    return;

  munmap(o->buffer, o->width * o->height * BPP);
  o->buffer = NULL;
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
