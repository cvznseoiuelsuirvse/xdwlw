#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-common.h"
#include "xdwayland-core.h"
#include "xdwayland-structs.h"
#include "xdwayland-utils.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>

xdwl_map *__xdwl_interfaces = NULL;
static xdwl_map *__xdwl_listeners = NULL;

static void xdwl_dispatch_message(xdwl_proxy *proxy,
                                  xdwl_raw_message *raw_message) {
  xdwl_object *object = object_get_by_id(proxy, raw_message->object_id);
  if (object == NULL) {
    xdwl_raise(proxy, "xdwl_dispatch_message", "no object found with id %d",
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

  struct xdwl_listener *listener =
      xdwl_map_get(__xdwl_listeners, raw_message->object_id);

  if (listener) {
    void **interface_ptr = listener->interface;

    xdwl_event_handler *handler = interface_ptr[raw_message->method_id];
    if (handler == NULL) {
      return;
    }

    handler(listener->user_data, args);
  }

  free(args);
};

static void destroy_objects(xdwl_proxy *proxy) {
  for (size_t i = 0; i < proxy->obj_reg->size; i++) {
    struct xdwl_map_pair *p = proxy->obj_reg->pairs[i];
    for (p = p; p; p = p->next) {
      xdwl_object *o = p->value;
      free(o->name);
    }
  }

  xdwl_map_destroy(proxy->obj_reg);
}

static void destroy_interfaces() {
  for (size_t i = 0; i < __xdwl_interfaces->size; i++) {
    struct xdwl_map_pair *p = __xdwl_interfaces->pairs[i];

    for (p = p; p; p = p->next) {
      struct xdwl_interface *interface = p->value;

      for (xdwl_list *l = interface->events; l->next; l = l->next) {
        struct xdwl_method *method = l->data;
        free(method->name);
        free(method->signature);
      }
      xdwl_list_destroy(interface->events);

      for (xdwl_list *l = interface->requests; l->next; l = l->next) {
        struct xdwl_method *method = l->data;
        free(method->name);
        free(method->signature);
      }
      xdwl_list_destroy(interface->requests);

      free(interface);
    }
  }
  xdwl_map_destroy(__xdwl_interfaces);
};

xdwl_proxy *xdwl_proxy_create() {
  size_t sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sock_addr = {.sun_family = AF_UNIX};

  char *display = getenv("WAYLAND_DISPLAY");
  if (display == NULL) {
    xdwl_raise(NULL, "xdwl_proxy_create", "WAYLAND_DISPLAY isn't set");
  }

  char *xdg_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_dir == NULL) {
    xdwl_raise(NULL, "xdwl_proxy_create", "XDG_RUNTIME_DIR isn't set");
  }

  size_t socket_path_len = strlen(display) + strlen(xdg_dir) + 2;

  char socket_path[socket_path_len];
  snprintf(socket_path, socket_path_len, "%s/%s", xdg_dir, display);
  memcpy(sock_addr.sun_path, socket_path, socket_path_len);

  int connect_status =
      connect(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr));

  if (connect_status < 0) {
    xdwl_raise(NULL, "xdwl_proxy_create", "failed to connect to %s", display);
  }

  xdwl_proxy *proxy = malloc(sizeof(xdwl_proxy));
  if (proxy == NULL) {
    xdwl_raise(NULL, "xdwl_proxy_create", "failed to allocate proxy");
  }

  proxy->sockfd = sock_fd;

  proxy->obj_reg = xdwl_map_new(CAP);
  if (proxy->obj_reg == NULL) {
    xdwl_raise(NULL, "xdwl_proxy_create",
               "failed to allocate object registry buffer");
  }

  proxy->buffer = malloc(CAP);
  if (proxy->buffer == NULL) {
    xdwl_raise(proxy, "xdwl_proxy_create", "failed to allocate proxy buffer");
  }

  __xdwl_listeners = xdwl_map_new(CAP);
  if (__xdwl_listeners == NULL) {
    xdwl_raise(proxy, "xdwl_proxy_create", "failed to allocate listeners map");
  }

  __xdwl_interfaces = xdwl_map_new(CAP);
  if (__xdwl_interfaces == NULL) {
    xdwl_raise(proxy, "xdwl_proxy_create", "failed to allocate interfaces map");
  }

  load_interfaces("/usr/share/wayland/wayland.xml");

  return proxy;
}

void xdwl_proxy_destroy(xdwl_proxy *proxy) {
  close(proxy->sockfd);

  if (proxy != NULL && proxy->buffer != NULL) {
    free(proxy->buffer);
  };

  if (proxy != NULL && proxy->obj_reg != NULL) {
    destroy_objects(proxy);
  }

  if (proxy != NULL) {
    free(proxy);
  }

  if (__xdwl_interfaces != NULL) {
    destroy_interfaces();
  }

  if (__xdwl_listeners != NULL) {
    xdwl_map_destroy(__xdwl_listeners);
  };
};

void xdwl_add_listener(xdwl_proxy *proxy, const char *object_name,
                       void *interface, void *user_data) {

  xdwl_object *object = object_get_by_name(proxy, object_name);
  if (!object) {
    xdwl_raise(proxy, "xdwl_add_listener",
               "no registered objects found with name %s", object_name);
  }

  struct xdwl_listener listener = {.user_data = user_data,
                                   .interface = interface};
  size_t object_id = object->id;
  xdwl_map_set(__xdwl_listeners, object_id, &listener,
               sizeof(struct xdwl_listener));
}

void xdwl_remove_listener(xdwl_proxy *proxy, char *object_name) {
  struct xdwl_listener *listener =
      xdwl_map_get_str(__xdwl_listeners, object_name);
  if (listener != NULL) {
    xdwl_map_remove_str(__xdwl_listeners, object_name);
  }
}

static ssize_t xdwl_sock_send(xdwl_proxy *proxy, size_t buffer_len, int fd) {
  if (fd > 0) {
    struct msghdr m;
    char cmsg[CMSG_SPACE(sizeof(fd))];

    memset(cmsg, 0, CMSG_SPACE(sizeof(fd)));
    memset(&m, 0, sizeof(struct msghdr));

    struct iovec e = {proxy->buffer, buffer_len};
    m.msg_iov = &e;
    m.msg_iovlen = 1;
    m.msg_control = cmsg;
    m.msg_controllen = CMSG_SPACE(sizeof(fd));

    struct cmsghdr *c = CMSG_FIRSTHDR(&m);
    c->cmsg_level = SOL_SOCKET;
    c->cmsg_type = SCM_RIGHTS;
    c->cmsg_len = CMSG_LEN(sizeof(fd));
    *(int *)CMSG_DATA(c) = fd;

    return sendmsg(proxy->sockfd, &m, 0);
  }
  return send(proxy->sockfd, proxy->buffer, buffer_len, 0);
};

void xdwl_send_request(xdwl_proxy *proxy, char *object_name, size_t method_id,
                       size_t arg_count, ...) {
  va_list args;
  const xdwl_object *object = object_get_by_name(proxy, object_name);
  if (!object) {
    xdwl_raise(proxy, "xdwl_send_request",
               "no registered objects found with name %s", object_name);
  }
  size_t object_id = object->id;

  struct xdwl_method *request =
      xdwl_list_get(object->interface->requests, method_id);

  if (request == NULL) {
    xdwl_raise(proxy, "xdwl_send_request",
               "couldn't find request method with id %d", method_id);
  }

  char *request_signature = request->signature;
  xdwl_arg request_args[arg_count];
  int fd = 0;

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

    case 'h':
      arg.fd = va_arg(args, int32_t);
      fd = arg.fd;
      break;
    }

    request_args[i] = arg;
  }

#ifdef LOGS
  xdwl_log("INFO", "-> %s.#%ld.%s", object_name, object_id, request->name);
  xdwl_show_args(request_args, request_signature);
#endif

  size_t message_size =
      HEADER_SIZE +
      calculate_body_size(request_args, arg_count, request_signature);
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
  int n = xdwl_sock_send(proxy, message_size, fd);
  if (n < 0) {
    perror("sendmsg");
    xdwl_raise(proxy, "xdwl_send_request", "failed to send message");
  }
  fflush(stdout);
};
static ssize_t xdwl_sock_recv(xdwl_proxy *proxy, int *fd) {
  char cmsg[CMSG_SPACE(sizeof(int))];
  struct iovec e = {proxy->buffer, CAP};
  struct msghdr m = {NULL, 0, &e, 1, cmsg, sizeof(cmsg), 0};

  ssize_t n = recvmsg(proxy->sockfd, &m, 0);
  struct cmsghdr *c = CMSG_FIRSTHDR(&m);
  if (c != NULL)
    *fd = *(int *)c;

  return n;
}

void xdwl_recv_events(xdwl_proxy *proxy, xdwl_list *messages_list) {
  int fd = 0;
  int n = xdwl_sock_recv(proxy, &fd);

  if (n <= 0) {
    xdwl_raise(proxy, "xdwl_recv_events", "server is gone");
  }

  int offset = 0;

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

    xdwl_raw_message raw_message = {
        .object_id = object_id,
        .method_id = method_id,
        .body_length = message_size - HEADER_SIZE,
        .fd = fd,
    };
    memcpy(raw_message.body, proxy->buffer + _offset, raw_message.body_length);

    xdwl_object *object = object_get_by_id(proxy, object_id);
    if (object == NULL) {
      xdwl_raise(proxy, "xdwl_recv_events",
                 "no registered objects found with id %d", object_id);
    }
    xdwl_list_push(messages_list, &raw_message,
                   sizeof(struct xdwl_raw_message));
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
  }

  xdwl_list_destroy(raw_messages);
};
