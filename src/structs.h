#include <stdint.h>
#include <unistd.h>

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
  int lwidth;
  int lheight;
};
