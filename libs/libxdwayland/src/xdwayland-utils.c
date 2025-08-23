#include "xdwayland-client.h"
#include "xdwayland-common.h"
#include "xdwayland-core.h"
#include "xdwayland-structs.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>

extern xdwl_map *interfaces;
static size_t auto_object_id = 0;

void xdwl_log(const char *level, const char *message, ...) {
  va_list args;
  time_t t;
  struct tm *tt;
  char timestamp_string[10];

  t = time(NULL);
  tt = localtime(&t);

  strftime(timestamp_string, sizeof(timestamp_string), "%H:%M:%S", tt);

  va_start(args, message);
  printf("[libxdwayland %s %s] ", timestamp_string, level);
  vprintf(message, args);

  va_end(args);
  fflush(stdout);
}

void xdwl_show_args(xdwl_arg *args, char *signature) {
  size_t argc = strlen(signature);
  printf("(");

  for (size_t i = 0; i < argc; i++) {
    char arg_type = signature[i];
    switch (arg_type) {
    case 'i':
      printf("%d", args[i].i);
      break;
    case 'u':
      printf("%d", args[i].u);
      break;
    case 'f':
      printf("%f", args[i].f);
      break;

    case 's':
      printf("\"%s\"", args[i].s);
      break;
    }
    if (i != argc - 1) {
      printf(", ");
    }
  }

  printf(")\n");
}

void xdwl_raise_error(xdwl_proxy *proxy, const char *error_type,
                      const char *message, ...) {
  va_list args;

  va_start(args, message);

  printf("%sError: ", error_type);
  vprintf(message, args);
  printf("\n");

  va_end(args);

  xdwl_proxy_destroy(proxy);
  exit(1);
}

xdwl_arg *read_args(xdwl_raw_message *message, const char *signature) {
  size_t arg_count = strlen(signature);
  xdwl_arg *args = malloc(arg_count * sizeof(xdwl_arg));

  size_t offset = 0;
  for (size_t i = 0; i < arg_count; i++) {
    char arg_signature = signature[i];
    switch (arg_signature) {
    case 'i':
      args[i].i = *(uint32_t *)(message->body + offset);
      offset += sizeof(uint32_t);
      break;

    case 'u':
      args[i].u = *(uint32_t *)(message->body + offset);
      offset += sizeof(uint32_t);
      break;

    case 'f':
      args[i].f = *(double *)(message->body + offset);
      offset += sizeof(double);
      break;

    case 's':
      offset += sizeof(uint32_t);

      args[i].s = (char *)(message->body + offset);
      offset += PADDED4(strlen(args[i].s));
      break;

    case 'h':
      args[i].fd = message->fd;
      break;
    }
  }

  return args;
};

void write_args(char *buffer, size_t *offset, xdwl_arg *args, size_t arg_count,
                char *signature) {
  size_t string_length;

  for (size_t i = 0; i < arg_count; i++) {
    char arg_signature = signature[i];
    xdwl_arg arg = args[i];
    switch (arg_signature) {
    case 'i':
      *(int32_t *)(buffer + *offset) = arg.i;
      *offset += sizeof(int32_t);
      break;

    case 'u':
      *(uint32_t *)(buffer + *offset) = arg.u;
      *offset += sizeof(uint32_t);
      break;

    case 'f':
      *(float *)(buffer + *offset) = arg.f;
      *offset += sizeof(float);
      break;

    case 's':
      string_length = strlen(arg.s);
      *(uint32_t *)(buffer + *offset) = string_length + 1;
      *offset += sizeof(uint32_t);

      string_length = PADDED4(string_length);
      memcpy((char *)(buffer + *offset), arg.s, string_length);
      *offset += string_length;
      break;
    }
  }
}

void buf_write_u32(void *buf, size_t *buf_size, uint32_t n) {
  *(uint32_t *)(buf + *buf_size) = n;
  *buf_size += sizeof(n);
}

void buf_write_u16(void *buf, size_t *buf_size, uint16_t n) {
  *(uint16_t *)(buf + *buf_size) = n;
  *buf_size += sizeof(n);
}

uint32_t buf_read_u32(void *buf, size_t *buf_size) {
  uint32_t value = *(uint32_t *)(buf + *buf_size);
  *buf_size += sizeof(value);

  return value;
}

uint16_t buf_read_u16(void *buf, size_t *buf_size) {
  uint16_t value = *(uint16_t *)(buf + *buf_size);
  *buf_size += sizeof(value);

  return value;
}

char *buf_read_string(void *buf, size_t *buf_size) {
  uint32_t string_length = *(uint32_t *)(buf + *buf_size);
  *buf_size += sizeof(string_length);

  char *string = (char *)(buf + *buf_size);
  return string;
}

uint16_t calculate_body_size(xdwl_arg *args, size_t arg_count,
                             char *signature) {
  uint16_t body_size = 0;

  for (size_t i = 0; i < arg_count; i++) {
    xdwl_arg arg = args[i];
    char arg_signature = signature[i];

    switch (arg_signature) {
    case 'i':
      body_size += sizeof(int32_t);
      break;

    case 'u':
      body_size += sizeof(uint32_t);
      break;

    case 'f':
      body_size += sizeof(float);
      break;

    case 's':
      body_size += sizeof(uint32_t);
      body_size += PADDED4(strlen(arg.s) + 1);
      break;
    }
  };

  return body_size;
}

xdwl_object *object_get_by_id(xdwl_proxy *proxy, size_t object_id) {
  xdwl_map_key k = {.type = 'i', .key.integer = object_id};
  xdwl_object *object = xdwl_map_get(proxy->id_to_obj_reg, k);

  if (!object) {
    return NULL;
  }

  return object;
}

xdwl_object *object_get_by_name(xdwl_proxy *proxy, const char *object_name) {
  xdwl_map_key k = {.type = 's', .key.string = object_name};
  xdwl_object *object = xdwl_map_get(proxy->name_to_obj_reg, k);

  if (!object) {
    return 0;
  }

  return object;
}

size_t object_register(xdwl_proxy *proxy, size_t object_id, char *object_name) {
  size_t o;
  if (object_id == 0) {
    auto_object_id++;
    o = auto_object_id;

  } else {
    o = object_id;
  }

  xdwl_map_key k1 = {.type = 's', .key.string = object_name};
  xdwl_map_key k2 = {.type = 'i', .key.integer = o};

  struct xdwl_interface *interface = xdwl_map_get(interfaces, k1);
  if (interface == NULL) {
    xdwl_raise_error(proxy, "Object", "Failed to register object %s.#%d",
                     object_name, o);
  }

  xdwl_object object = {.id = o, .interface = interface};
  object.name = malloc(strlen(object_name + 1));
  strcpy(object.name, object_name);

  xdwl_map_set(proxy->name_to_obj_reg, k1, &object, sizeof(xdwl_object));
  xdwl_map_set(proxy->id_to_obj_reg, k2, &object, sizeof(xdwl_object));

  return o;
}

void object_unregister(xdwl_proxy *proxy, size_t object_id) {
  xdwl_object *object = object_get_by_id(proxy, object_id);

  if (object) {
    // if object_id is client allocated
    if (object_id < 0xFF000000) {
      auto_object_id--;
    }

    xdwl_map_key k1 = {.type = 'i', .key.integer = object_id};
    xdwl_map_key k2 = {.type = 's', .key.string = object->name};

    free(object->name);
    xdwl_map_unset(proxy->id_to_obj_reg, k1);
    xdwl_map_unset(proxy->name_to_obj_reg, k2);
  }
}
