#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define STREQ(s1, s2) strcmp(s1, s2) == 0
#define COLOR(hex)                                                             \
  {((hex >> 24) & 0xFF), ((hex >> 16) & 0xFF), ((hex >> 8) & 0xFF),            \
   (hex & 0xFF)}

enum non_core_interfaces {
  WP_VIEWPORTER = 1,
  ZWLR_LAYER_SHELL_V1 = 1 << 1,
  ZXDG_OUTPUT_MANAGER_V1 = 1 << 2,
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

enum ipc_errors {
  IPC_ERR_UNSUPPORTED_FORMAT = 0x10,
  IPC_ERR_INVALID_COLOR = 0x20,
  IPC_ERR_INVALID_IMAGE_FIT_MODE = 0x30,
  IPC_ERR_IMAGE_NOT_FOUND = 0x40,
};

struct wl_global {
  uint32_t name;
  char *interface;
  uint32_t version;
};

struct output {
  int id;
  char *name;
  uint32_t width;
  uint32_t height;
  uint32_t logical_width;
  uint32_t logical_height;
  int fd;
  bool busy;
};

struct ipc_message {
  enum ipc_message_types type;
  union {
    struct {
      enum ipc_errors code;
      const char *msg;
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
