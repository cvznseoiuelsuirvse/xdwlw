#include "hashmap.h"
#include <stdio.h>

int main() {
  map *m = map_new(1024);

  map_key k1 = {.type = 's', .key.string = "wl_display"};
  map_key k2 = {.type = 's', .key.string = "wl_registry"};
  size_t v1 = 1;
  size_t v2 = 2;

  map_set(m, k1, &v1);
  map_set(m, k2, &v2);

  size_t *value = map_get(m, k2);
  printf("%ld\n", *value);

exit:
  map_destroy(m);
  return 0;
}
