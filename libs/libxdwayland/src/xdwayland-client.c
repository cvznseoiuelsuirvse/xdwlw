#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-common.h"
#include "xdwayland-core.h"
#include "xdwayland-structs.h"
#include "xdwayland-utils.h"

#include <stdarg.h>
#include <stdint.h>
#include <sys/socket.h>

xdwl_map *interfaces;
static xdwl_map *listeners;

static void xdwl_dispatch_message(xdwl_proxy *proxy,
                                  xdwl_raw_message *raw_message) {
  xdwl_map_key k = {.type = 'i', .key.integer = raw_message->object_id};

  xdwl_object *object = object_get_by_id(proxy, raw_message->object_id);
  if (object == NULL) {
    xdwl_raise_error(proxy, "Object", "No object found with id %d",
                     raw_message->object_id);
  }

  struct xdwl_method *event =
      xdwl_list_get(object->interface->events, raw_message->method_id);

  char *event_signature = event->signature;
  xdwl_arg *args = read_args(raw_message, event_signature);

#ifdef LOGS
  const char *object_name = object->name;
  xdwl_log("INFO", "<- %s.#%ld.%s", object_name, raw_message->object_id,
           event->name);
  xdwl_show_args(args, event_signature);
#endif

  struct xdwl_listener *listener = xdwl_map_get(listeners, k);

  if (listener) {
    void **handler_ptr = listener->handler;

    xdwl_event_handler *handler = handler_ptr[raw_message->method_id];
    if (handler == NULL) {
      return;
    }

    handler(listener->user_data, args);
    free(args);
  }
};

static void destroy_interfaces() {
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

xdwl_proxy *xdwl_proxy_create() {
  size_t sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sock_addr = {.sun_family = AF_UNIX};

  const char *display = getenv("WAYLAND_DISPLAY");
  const char *xdg_dir = getenv("XDG_RUNTIME_DIR");

  size_t socket_path_len = strlen(display) + strlen(xdg_dir) + 2;

  char *socket_path = malloc(socket_path_len);
  snprintf(socket_path, socket_path_len, "%s/%s", xdg_dir, display);

  memcpy(sock_addr.sun_path, socket_path, socket_path_len);

  int connect_status =
      connect(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));

  if (connect_status < 0) {
    perror("connect");
    exit(connect_status);
  }

  listeners = xdwl_map_new(CAP);
  interfaces = xdwl_map_new(CAP);

  load_interfaces("/usr/share/wayland/wayland.xml");

  xdwl_proxy *proxy = malloc(sizeof(xdwl_proxy));

  proxy->sockfd = sock_fd;
  proxy->id_to_obj_reg = xdwl_map_new(CAP);
  proxy->name_to_obj_reg = xdwl_map_new(CAP);
  proxy->buffer = calloc(1, CAP);

  return proxy;
}

void xdwl_proxy_destroy(xdwl_proxy *proxy) {
  close(proxy->sockfd);

  free(proxy->buffer);

  xdwl_map_destroy(proxy->id_to_obj_reg);
  xdwl_map_destroy(proxy->name_to_obj_reg);

  free(proxy);

  destroy_interfaces();
  xdwl_map_destroy(interfaces);

  xdwl_map_destroy(listeners);
};

void xdwl_add_listener(xdwl_proxy *proxy, const char *object_name,
                       void *interface, void *user_data) {

  struct xdwl_listener listener = {.handler = interface,
                                   .user_data = user_data};

  xdwl_object *object = object_get_by_name(proxy, object_name);
  if (!object) {
    xdwl_raise_error(proxy, "Object",
                     "No registered objects found with name %s", object_name);
  }

  size_t bind_to = object->id;
  xdwl_map_key key = {.type = 'i', .key.integer = bind_to};
  xdwl_map_set(listeners, key, &listener, sizeof(struct xdwl_listener));
}

static void xdwl_sock_send(xdwl_proxy *proxy, size_t buffer_len, int fd) {
  char cmsg[CMSG_SPACE(sizeof(int))];

  struct iovec e = {proxy->buffer, buffer_len};
  struct msghdr m = {NULL, 0, &e, 1, cmsg, sizeof(cmsg), 0};

  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  c->cmsg_len = CMSG_LEN(sizeof(int));
  c->cmsg_level = SOL_SOCKET;
  c->cmsg_type = SCM_RIGHTS;
  *(int *)CMSG_DATA(c) = fd;

  sendmsg(proxy->sockfd, &m, 0);
};

void xdwl_send_request(xdwl_proxy *proxy, char *object_name, size_t method_id,
                       int fd, size_t arg_count, ...) {
  va_list args;
  const xdwl_object *object = object_get_by_name(proxy, object_name);
  size_t object_id = object->id;

  if (!object) {
    object_id = object_register(proxy, 0, object_name);
  }

  struct xdwl_method *request =
      xdwl_list_get(object->interface->requests, method_id);
  if (request == NULL) {
    xdwl_raise_error(proxy, "Client", "Couldn't find request method with id %d",
                     method_id);
  }

  char *request_signature = request->signature;
  xdwl_arg request_args[arg_count];

  va_start(args, arg_count);

  for (size_t i = 0; i < arg_count; i++) {
    xdwl_arg arg;
    char arg_signature = request_signature[i];

    switch (arg_signature) {
    case 'i':
      arg.i = va_arg(args, int32_t);
      break;

    case 'u':
      arg.u = va_arg(args, uint32_t);
      break;

    case 'f':
      arg.f = va_arg(args, double);
      break;

    case 's':
      arg.s = va_arg(args, char *);
      break;
    }

    request_args[i] = arg;
  }

#ifdef LOGS
  xdwl_log("INFO", "-> %s.#%ld.%s", object_name, object_id, request->name);
  xdwl_show_args(request_args, request_signature);
#endif

  size_t message_size =
      8 + calculate_body_size(request_args, arg_count, request_signature);
  size_t offset = 0;

  buf_write_u32(proxy->buffer, &offset, object_id);
  buf_write_u16(proxy->buffer, &offset, method_id);
  buf_write_u16(proxy->buffer, &offset, message_size);
  write_args(proxy->buffer, &offset, request_args, arg_count,
             request_signature);

#ifdef DEBUG
  printf("\nTO SERVER\n");
  for (size_t i = 0; i < message_size; i++) {
    char c = proxy->buffer[i];
    printf("%02X ", c);
  }
  printf("\nTO SERVER\n");
#endif
  xdwl_sock_send(proxy, message_size, fd);
  fflush(stdout);
};
static int xdwl_sock_recv(xdwl_proxy *proxy, int *fd) {
  char cmsg[CMSG_SPACE(sizeof(int))];
  struct iovec e = {proxy->buffer, CAP};
  struct msghdr m = {NULL, 0, &e, 1, cmsg, sizeof(cmsg), 0};

  int n = recvmsg(proxy->sockfd, &m, 0);
  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  fd = (int *)c;

  return n;
}

void xdwl_recv_events(xdwl_proxy *proxy, xdwl_list *messages_list) {
  int fd = 0;
  int n = xdwl_sock_recv(proxy, &fd);

  if (n <= 0) {
    xdwl_raise_error(proxy, "Server", "Server is gone");
  }

  size_t offset = 0;

#ifdef DEBUG
  printf("\nFROM SERVER\n");
  for (size_t i = 0; i < n; i++) {
    char c = proxy->buffer[offset + i];
    printf("%02X ", c);
  }
  printf("\nFROM SERVER\n");
#endif

  while (offset + HEADER_SIZE <= n) {
    size_t _offset = offset;

    size_t object_id = buf_read_u32(proxy->buffer, &_offset);
    uint16_t method_id = buf_read_u16(proxy->buffer, &_offset);
    uint16_t message_size = buf_read_u16(proxy->buffer, &_offset);

    xdwl_raw_message *raw_message = malloc(sizeof(xdwl_raw_message));

    raw_message->object_id = object_id;
    raw_message->method_id = method_id;
    raw_message->body_length = message_size - HEADER_SIZE;
    raw_message->body = proxy->buffer + _offset;
    raw_message->fd = fd;

    xdwl_object *object = object_get_by_id(proxy, object_id);
    if (object == NULL) {
      xdwl_raise_error(proxy, "Object",
                       "No registered objects found with id %d", object_id);
    }
    xdwl_list_append(messages_list, raw_message);
    offset += message_size;
  }
};

void xdwl_roundtrip(xdwl_proxy *proxy) {
  size_t callback = object_register(proxy, 0, "wl_callback");

  xdwl_display_sync(proxy, callback);

  uint8_t loop = 1;

  while (loop) {
    xdwl_list *raw_messages = xdwl_list_new();
    xdwl_recv_events(proxy, raw_messages);

    size_t msgc = xdwl_list_len(raw_messages);

    for (size_t i = 0; i < msgc; i++) {
      xdwl_raw_message *raw_message = xdwl_list_get(raw_messages, i);
      xdwl_dispatch_message(proxy, raw_message);
      if (raw_message->object_id == callback) {
        loop = 0;
      }
      free(raw_message);
    }
    xdwl_list_destroy(raw_messages);
  }
}
void xdwl_dispatch(xdwl_proxy *proxy) {
  xdwl_list *raw_messages = xdwl_list_new();
  xdwl_recv_events(proxy, raw_messages);

  size_t msgc = xdwl_list_len(raw_messages);

  for (size_t i = 0; i < msgc; i++) {
    xdwl_raw_message *raw_message = xdwl_list_get(raw_messages, i);

    xdwl_dispatch_message(proxy, raw_message);
    free(raw_message);
  }

  xdwl_list_destroy(raw_messages);
};
