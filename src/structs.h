#ifndef STRUCTS_H
#define STRUCTS_H

#include <stdint.h>
#include <unistd.h>

#define STREQ(s1, s2) strcmp(s1, s2) == 0

struct wl_global {
  uint32_t name;
  char *interface;
  uint32_t version;
};

struct output {
  int id;
  char *name;
  int width;
  int height;
  int logical_width;
  int logical_height;
};

enum non_core_interfaces {
  WP_VIEWPORTER = 1,
  ZWLR_LAYER_SHELL_V1 = 1 << 1,
  ZXDG_OUTPUT_MANAGER_V1 = 1 << 2,
};

#endif
