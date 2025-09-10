#ifndef XDWLW_TYPES_H
#define XDWLW_TYPES_H

#include "xdwlw-common.h"

#define STREQ(s1, s2) strcmp(s1, s2) == 0
#define COLOR(hex)                                                             \
  {((hex >> 24) & 0xFF), ((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF),            \
   (hex & 0xFF)}

enum non_core_interfaces {
  WP_VIEWPORTER = 1,
  ZWLR_LAYER_SHELL_V1 = 1 << 1,
  ZXDG_OUTPUT_MANAGER_V1 = 1 << 2,
  ZWLR_OUTPUT_POWER_MANAGER_V1 = 1 << 3,
};

enum image_modes {
  IMAGE_MODE_CENTER = 'C',
  IMAGE_MODE_FIT = 'F',
};

enum ipc_message_types {
  // general messages (server + client)
  IPC_UNKNOWN = 0x0,
  IPC_ACK = 0x1,

  // client messages (only from client to server)
  IPC_CLIENT_KILL = 0x3,
  IPC_CLIENT_SET_IMAGE = 0x4,
  IPC_CLIENT_SET_COLOR = 0x5,

  // server messages (only from server to client)
  IPC_SERVER_ERR = 0x2,
};

struct wl_global {
  uint32_t name;
  char *interface;
  uint32_t version;
};

struct output {
  uint32_t id;
  char *name;

  int color;
  const char *image_path;
  char image_mode;

  uint32_t width;
  uint32_t height;
  uint32_t logical_width;
  uint32_t logical_height;

  uint32_t *buffer;
  uint32_t buffer_id;
  uint32_t surface_id;
};

struct ipc_message {
  enum ipc_message_types type;
  union {
    struct {
      char msg[1024];
    } error;

    struct {
      const char *output;
      uint32_t color;
    } set_color;

    struct {
      const char *output;
      enum image_modes mode;
      const char *path;
    } set_image;
  };
};

#endif
