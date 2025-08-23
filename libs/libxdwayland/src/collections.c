#include "common.h"
#include <stdlib.h>

typedef struct xdwl_map_key {
  char type;
  union key {
    char *string;
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

static size_t djb2(char *string) {
  size_t hash = 5381;
  int c;

  while ((c = *string++))
    hash = ((hash << 5) + hash) + c;

  return hash;
}

static inline uint8_t string_cmp(xdwl_map_key k1, xdwl_map_key k2) {
  return k1.key.string && (strcmp(k1.key.string, k2.key.string) == 0);
};
static inline uint8_t int_cmp(xdwl_map_key k1, xdwl_map_key k2) {
  return k1.key.integer && k1.key.integer == k2.key.integer;
};

xdwl_map *xdwl_map_new(size_t capacity) {
  xdwl_map *m = malloc(sizeof(xdwl_map));
  if (!m)
    return NULL;

  m->cap = capacity;
  m->buckets = calloc(m->cap, sizeof(*m->buckets));

  if (!m->buckets) {
    free(m);
    return NULL;
  }

  return m;
}
void xdwl_map_destroy(const xdwl_map *m) {
  if (!m)
    return;

  for (size_t i = 0; i < m->cap; i++) {
    struct xdwl_bucket *b = m->buckets[i];

    while (b) {
      struct xdwl_bucket *next = b->next;
      free(b->value);
      free(b);
      b = next;
    }
  };

  free(m->buckets);
  free((xdwl_map *)m);
}

void xdwl_map_set(const xdwl_map *m, xdwl_map_key key, void *value,
                  size_t value_size) {
  size_t index;
  uint8_t (*cmp_func)(xdwl_map_key, xdwl_map_key);

  switch (key.type) {
  case 's':
    index = djb2(key.key.string);
    cmp_func = string_cmp;

    break;

  case 'i':
    index = key.key.integer;
    cmp_func = int_cmp;
    break;

  default:
    fprintf(stderr, "Invalid key type: %c", key.type);
    exit(1);
  }

  index = index % m->cap;
  struct xdwl_bucket *b = m->buckets[index];

  while (b) {
    if (cmp_func(b->key, key)) {
      free(b->value);

      b->value = malloc(value_size);
      memcpy(b->value, value, value_size);

      return;
    }
    if (!b->next)
      break;
    b = b->next;
  }

  struct xdwl_bucket *new_b = malloc(sizeof(struct xdwl_bucket));

  if (!new_b) {
    perror("malloc");
    exit(1);
  }

  new_b->value = malloc(value_size);
  memcpy(new_b->value, value, value_size);

  new_b->key = key;
  new_b->next = m->buckets[index];

  m->buckets[index] = new_b;
};

void xdwl_map_unset(const xdwl_map *m, xdwl_map_key key) {
  size_t index;
  uint8_t (*cmp_func)(xdwl_map_key, xdwl_map_key);

  switch (key.type) {
  case 's':
    index = djb2(key.key.string);
    cmp_func = string_cmp;
    break;

  case 'i':
    index = key.key.integer;
    cmp_func = int_cmp;
    break;

  default:
    fprintf(stderr, "Invalid key type: %c", key.type);
    exit(1);
  }

  index = index % m->cap;
  struct xdwl_bucket *b = m->buckets[index];
  struct xdwl_bucket *prev = NULL;

  for (struct xdwl_bucket *bucket = b; bucket; bucket = bucket->next) {
    if (cmp_func(bucket->key, key)) {
      if (prev) {
        prev->next = bucket->next;
      } else {
        m->buckets[index] = bucket->next;
      }

      free(b->value);
      free(b);

    } else {
      prev = bucket;
    }
  }
}

// struct xdwl_bucket *prev = NULL;
//
// if (!b)
//   return;
//
// while (b) {
//   if (cmp_func(b->key, key)) {
//     if (prev) {
//       prev->next = b->next;
//     } else {
//       m->buckets[index] = b->next;
//     }
//
//     free(b->value);
//     free(b);
//     return;
//   }
//
//   prev = b;
//   b = b->next;
// }
//
void *xdwl_map_get(const xdwl_map *m, xdwl_map_key key) {
  size_t index;
  uint8_t (*cmp_func)(xdwl_map_key, xdwl_map_key);

  switch (key.type) {
  case 's':
    index = djb2(key.key.string);
    cmp_func = string_cmp;

    break;

  case 'i':
    index = key.key.integer;
    cmp_func = int_cmp;
    break;

  default:
    fprintf(stderr, "Invalid key type: %c", key.type);
    exit(1);
  }

  index = index % m->cap;
  struct xdwl_bucket *b = m->buckets[index];

  if (!b)
    return NULL;

  while (b) {
    if (cmp_func(b->key, key))
      return b->value;
    b = b->next;
  }

  return NULL;
}

xdwl_list *xdwl_list_new() {
  xdwl_list *l = malloc(sizeof(xdwl_list));
  if (!l) {
    perror("malloc");
    exit(1);
  }
  l->empty = 1;
  l->next = NULL;
  return l;
};

void xdwl_list_destroy(xdwl_list *l) {
  while (l) {
    xdwl_list *next = l->next;
    free(l);
    l = next;
  }
};

void xdwl_list_append(xdwl_list *l, void *value) {
  xdwl_list *current_list;
  for (current_list = l; current_list->next; current_list = current_list->next)
    ;

  if (current_list->empty == 1) {
    current_list->empty = 0;
    current_list->value = value;

  } else {
    xdwl_list *new_list = xdwl_list_new();
    current_list->next = new_list;
    current_list->next->empty = 0;
    current_list->next->value = value;
  }
};

void *xdwl_list_get(xdwl_list *l, size_t index) {
  size_t current_index = 0;

  while (l) {
    if (current_index == index)
      return l->value;
    current_index++;
    l = l->next;
  }
  return NULL;
}

size_t xdwl_list_len(xdwl_list *l) {
  size_t i = 0;
  while (l && !l->empty) {
    l = l->next;
    i++;
  }

  return i;
}
