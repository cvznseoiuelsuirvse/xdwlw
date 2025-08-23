#ifndef COLLECTIONS_H
#define COLLECTIONS_H

#include "common.h"

typedef struct xdwl_map_key {
  char type;
  union key {
    const char *string;
    int integer;
  } key;
} xdwl_map_key;

struct xdwl_bucket {
  xdwl_map_key key;
  void *value;
  struct xdwl_bucket *prev;
  struct xdwl_bucket *next;
};

typedef struct xdwl_map {
  struct xdwl_bucket **buckets;
  size_t cap;
} xdwl_map;

typedef struct xdwl_list {
  uint8_t empty;
  void *value;
  struct xdwl_list *next;
} xdwl_list;

xdwl_map *xdwl_map_new(size_t capacity);
void xdwl_map_destroy(xdwl_map *m);
void xdwl_map_set(xdwl_map *m, xdwl_map_key key, void *value,
                  size_t value_size);
void xdwl_map_unset(xdwl_map *m, xdwl_map_key key);
void *xdwl_map_get(xdwl_map *m, xdwl_map_key key);

xdwl_list *xdwl_list_new();
void xdwl_list_destroy(xdwl_list *l);
void xdwl_list_append(xdwl_list *l, void *value);
void *xdwl_list_get(xdwl_list *l, size_t index);
size_t xdwl_list_len(xdwl_list *l);

#endif
