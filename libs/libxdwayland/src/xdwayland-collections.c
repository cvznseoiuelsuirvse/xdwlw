#include "xdwayland-common.h"

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

static size_t hash_string(char *string) {
  size_t hash = 5381;
  int c;

  while ((c = *string++))
    hash = ((hash << 5) + hash) + c;

  return hash;
}

xdwl_map *xdwl_map_new(size_t size) {
  xdwl_map *m = malloc(sizeof(xdwl_map));
  if (m == NULL)
    return NULL;

  m->size = size;
  m->pairs = calloc(size, sizeof(struct xdwl_map_pair *));
  if (m->pairs == NULL) {
    free(m);
    return NULL;
  }

  return m;
}

void xdwl_map_destroy(xdwl_map *m) {
  for (size_t i = 0; i < m->size; i++) {
    struct xdwl_map_pair *p = m->pairs[i];
    while (p) {
      struct xdwl_map_pair *next = p->next;
      free(p->value);
      free(p);
      p = next;
    }
  }
}

void *xdwl_map_set(xdwl_map *m, size_t key, void *value, size_t value_size) {
  size_t index = key % m->size;

  struct xdwl_map_pair *new_p = malloc(sizeof(struct xdwl_map_pair));
  struct xdwl_map_pair *recent_p = m->pairs[index];

  new_p->prev = NULL;
  new_p->next = recent_p;
  new_p->key = key;
  new_p->value = malloc(value_size);
  memcpy(new_p->value, value, value_size);

  if (recent_p != NULL) {
    recent_p->prev = new_p;
  }

  m->pairs[index] = new_p;

  return new_p->value;
};

void *xdwl_map_set_str(xdwl_map *m, char *key_str, void *value,
                       size_t value_size) {
  size_t key = hash_string(key_str);
  return xdwl_map_set(m, key, value, value_size);
};

void xdwl_map_remove(xdwl_map *m, size_t key) {
  size_t index = key % m->size;
  struct xdwl_map_pair *p = m->pairs[index];

  if (p != NULL) {
    size_t i;
    for (p = p, i = 0; p; p = p->next, i++) {
      if (p->key == key) {
        if (p->prev) {
          p->prev->next = p->next;
        } else {
          m->pairs[index] = p->next;
        }

        if (p->next) {
          p->next->prev = p->prev;
        }

        free(p->value);
        break;
      }
    }
  }
};

void xdwl_map_remove_str(xdwl_map *m, char *key_str) {
  size_t key = hash_string(key_str);
  return xdwl_map_remove(m, key);
}

void *xdwl_map_get(xdwl_map *m, size_t key) {
  size_t index = key % m->size;
  struct xdwl_map_pair *p = m->pairs[index];

  if (p != NULL) {
    size_t i;
    for (p = p, i = 0; p; p = p->next, i++) {
      if (p->key == key) {
        return p->value;
      }
    }
  }

  return NULL;
};

void *xdwl_map_get_str(xdwl_map *m, char *key_str) {
  size_t key = hash_string(key_str);
  return xdwl_map_get(m, key);
}

typedef struct xdwl_list {
  void *data;
  struct xdwl_list *prev;
  struct xdwl_list *next;
} xdwl_list;

void *xdwl_list_new() {
  xdwl_list *l = malloc(sizeof(xdwl_list));
  if (l == NULL)
    return NULL;

  l->data = NULL;
  l->next = NULL;
  l->prev = NULL;

  return l;
}

void xdwl_list_destroy(xdwl_list *l) {
  while (l) {
    xdwl_list *next = l->next;
    free(l->data);
    free(l);
    l = next;
  }
}

void *xdwl_list_push(xdwl_list *l, void *data, size_t data_size) {
  for (l = l; l->next; l = l->next)
    ;

  l->next = xdwl_list_new();
  l->next->prev = l;

  l->data = malloc(data_size);
  memcpy(l->data, data, data_size);

  return l->data;
}

void xdwl_list_remove(xdwl_list **head, size_t index) {
  size_t i;
  xdwl_list *l = *head;

  for (l = l, i = 0; l; l = l->next, i++) {
    if (index == i) {
      if (l->prev) {
        l->prev->next = l->next;
      } else {
        *head = l->next;
      }

      if (l->next) {
        l->next->prev = l->prev;
      }

      free(l->data);
      free(l);
      break;
    }
  }
}

void *xdwl_list_get(xdwl_list *l, size_t index) {
  size_t i;
  for (l = l, i = 0; l->next; l = l->next, i++) {
    if (index == i) {
      return l->data;
    }
  }

  return NULL;
}

size_t xdwl_list_len(xdwl_list *l) {
  size_t i;
  for (l = l, i = 0; l->next; l = l->next, i++)
    ;
  return i;
}
