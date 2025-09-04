#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "structs.h"
#include <fcntl.h>
#include <getopt.h>
#include <linux/limits.h>

void print_help() { printf("Usage: xdwlw -i <image> -m <[C]enter, [F]it>\n"); }

int main(int argc, char **argv) {
  if (argc < 2) {
    print_help();
    return 1;
  }

  int opt;
  const char *rpath = NULL;
  char apath[PATH_MAX];
  enum image_modes mode = 0;

  while ((opt = getopt(argc, argv, "hi:m:")) != -1) {
    switch (opt) {
    case 'i':
      rpath = optarg;
      break;
    case 'm':
      mode = *optarg;
      break;

    case 'h':
      print_help();
      return 1;
    }
  }

  if (rpath == NULL || mode == 0) {
    print_help();
    return 1;
  }

  if (!realpath(rpath, apath)) {
    perror("realpath");
    return 1;
  }
}
