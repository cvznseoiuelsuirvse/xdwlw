#include "collections.h"
#include "proto-parser.h"
#include "structs.h"
#include <stdio.h>

static void destroy_interfaces(xdwl_map *interfaces) {
  for (size_t i = 0; i < interfaces->cap; i++) {
    struct xdwl_bucket *b = interfaces->buckets[i];

    while (b) {
      struct xdwl_interface *interface = b->value;

      for (xdwl_list *p = interface->events; p; p = p->next) {
        if (p->empty == 0) {
          struct xdwl_method *method = p->value;
          free(method->name);
          free(method->signature);
          free(method);
        }
      }
      xdwl_list_destroy(interface->events);

      for (xdwl_list *p = interface->requests; p; p = p->next) {
        if (p->empty == 0) {
          struct xdwl_method *method = p->value;
          free(method->name);
          free(method->signature);
          free(method);
        }
      }

      xdwl_list_destroy(interface->requests);
      free(interface);

      b = b->next;
    }
  };
};

int main() {
  const char *xml_path = "/usr/share/wayland/wayland.xml";
  xdwl_map *m = xdwl_map_new(CAP);
  parse(xml_path, m);
  destroy_interfaces(m);
}
