#ifndef XDWAYLAND_COLLECTIONS_H
#define XDWAYLAND_COLLECTIONS_H

#include "xdwayland-common.h"
#include <stdint.h>

#define xdwl_list_for_each(pos, head, member)                                  \
  for (pos = (head); pos->next && ((member = pos->data), 1); pos = pos->next)

struct xdwl_map_pair {
  size_t key;
  void *value;
  struct xdwl_map_pair *prev;
  struct xdwl_map_pair *next;
};

typedef struct xdwl_map {
  struct xdwl_map_pair **pairs;
  size_t size;
} xdwl_map;

xdwl_map *xdwl_map_new(size_t size);
void xdwl_map_destroy(xdwl_map *m);
__must_check void *xdwl_map_set(xdwl_map *m, size_t key, void *value,
                                size_t value_size);
__must_check void *xdwl_map_set_str(xdwl_map *m, const char *key_str,
                                    void *value, size_t value_size);
void xdwl_map_remove(xdwl_map *m, size_t key);
void xdwl_map_remove_str(xdwl_map *m, const char *key_str);
void *xdwl_map_get(xdwl_map *m, size_t key);
void *xdwl_map_get_str(xdwl_map *m, const char *key_str);

typedef struct xdwl_list {
  void *data;
  struct xdwl_list *prev;
  struct xdwl_list *next;
} xdwl_list;

xdwl_list *xdwl_list_new();
void xdwl_list_destroy(xdwl_list *l);
__must_check void *xdwl_list_push(xdwl_list *l, void *data, size_t data_size);
void xdwl_list_remove(xdwl_list **head, size_t n);
__must_check void *xdwl_list_get(xdwl_list *l, size_t n);
size_t xdwl_list_len(xdwl_list *l);

typedef struct xdwl_bitmap {
  uint8_t *bytes;
  size_t size;
  size_t limit;
} xdwl_bitmap;

xdwl_bitmap *xdwl_bitmap_new(uint32_t limit);
void xdwl_bitmap_destroy(xdwl_bitmap *bm);
__must_check int xdwl_bitmap_chsize(xdwl_bitmap *bm, uint32_t new_size);
__must_check int xdwl_bitmap_set(xdwl_bitmap *bm, uint32_t n);
__must_check uint8_t xdwl_bitmap_get(xdwl_bitmap *bm, uint32_t n);
__must_check uint32_t xdwl_bitmap_get_free(xdwl_bitmap *bm);
__must_check int xdwl_bitmap_unset(xdwl_bitmap *bm, uint32_t n);

#endif
