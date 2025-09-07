#include "xdwayland-client.h"
#include "xdwayland-collections.h"
#include "xdwayland-core.h"
#include "xdwayland-private.h"
#include "xdwayland-types.h"

#define CLIENT_IDS_START 1
#define CLIENT_IDS_END 0xFEFFFFFF
#define MAX_CLIENT_IDS_SIZE ((CLIENT_IDS_END - CLIENT_IDS_START) + 1)

#define SERVER_IDS_START 0xFF000000
#define SERVER_IDS_END 0xFFFFFFFF
#define MAX_SERVER_IDS_SIZE ((SERVER_IDS_END - SERVER_IDS_START) + 1)

static xdwl_map *__xdwl_interfaces = NULL;
static xdwl_map *__xdwl_listeners = NULL;

static xdwl_bitmap *__client_id_pool = NULL;
static xdwl_bitmap *__server_id_pool = NULL;

__must_check static int xdwl_dispatch_message(xdwl_proxy *proxy,
                                              xdwl_raw_message *raw_message) {
  xdwl_object *object = xdwl_object_get_by_id(proxy, raw_message->object_id);
  if (object == NULL) {
    xdwl_error_set(XDWLERR_NULLOBJ,
                   "xdwl_dispatch_message: no object found with id %d",
                   raw_message->object_id);
    return -1;
  }

  struct xdwl_method *event =
      xdwl_list_get(object->interface->events, raw_message->method_id);

  if (event == NULL) {
    xdwl_error_set(XDWLERR_NULLEVENT,
                   "xdwl_dispatch_message: no event found for %s#%ld",
                   object->name, raw_message->method_id);
    return -1;
  }

  char *event_signature = event->signature;
  xdwl_arg *args = xdwl_read_args(raw_message, event_signature);

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
      return 0;
    }

    handler(listener->user_data, args);
  }

  free(args);
  return 0;
};

static void destroy_objects(xdwl_proxy *proxy) {
  for (size_t i = 0; i < proxy->obj_reg->size; i++) {
    struct xdwl_map_pair *p = proxy->obj_reg->pairs[i];
    for (; p; p = p->next) {
      xdwl_object *o = p->value;
      free(o->name);
    }
  }

  xdwl_map_destroy(proxy->obj_reg);
}

static void destroy_interfaces() {
  for (size_t i = 0; i < __xdwl_interfaces->size; i++) {
    struct xdwl_map_pair *p = __xdwl_interfaces->pairs[i];

    for (; p; p = p->next) {
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

__must_check static int xdwl_object_init() {
  __client_id_pool = xdwl_bitmap_new(MAX_CLIENT_IDS_SIZE);
  if (!__client_id_pool)
    return -1;

  __server_id_pool = xdwl_bitmap_new(MAX_SERVER_IDS_SIZE);
  if (!__server_id_pool)
    return -1;

  return 0;
}

static void xdwl_object_destroy() {
  xdwl_bitmap_destroy(__client_id_pool);
  xdwl_bitmap_destroy(__server_id_pool);
}

xdwl_object *xdwl_object_get_by_id(xdwl_proxy *proxy, uint32_t object_id) {
  xdwl_object *object = xdwl_map_get(proxy->obj_reg, object_id);

  if (!object) {
    return NULL;
  }

  return object;
}

xdwl_object *xdwl_object_get_by_name(xdwl_proxy *proxy,
                                     const char *object_name) {
  xdwl_object *object = NULL;

  for (size_t i = 0; i < proxy->obj_reg->size; i++) {
    struct xdwl_map_pair *p = proxy->obj_reg->pairs[i];
    for (; p; p = p->next) {
      xdwl_object *o = p->value;
      if (strcmp(o->name, object_name) == 0) {
        object = o;
      }
    }
  }

  return object;
}

int xdwl_object_register(xdwl_proxy *proxy, uint32_t object_id,
                         const char *object_name) {
  uint32_t o = object_id;
  int bit;

  if (object_id == 0) {
    if ((o = xdwl_bitmap_get_free(__client_id_pool)) == 0) {
    };

    if (xdwl_bitmap_set(__client_id_pool, o) == -1)
      return -1;

  } else if (object_id >= SERVER_IDS_START &&
             object_id <= SERVER_IDS_END) { // server id
    bit = xdwl_bitmap_get(__server_id_pool, object_id);

    /* if bitmap is too small */
    if (bit == -1 && xdwl_error_get_code() == XDWLERR_OUTOFRANGE) {
      /* if failed to grow bitmap */
      if (xdwl_bitmap_chsize(
              __server_id_pool,
              (uint32_t)((object_id - SERVER_IDS_START) / 8) * 8 + 8) == -1)
        return -1;

    } else if (bit == 1) {
      xdwl_error_set(XDWLERR_IDTAKEN,
                     "xdwl_object_register: failed to register server object "
                     "with id %ld. %ld is already taken",
                     object_id);
      return -1;
    }

    if (xdwl_bitmap_set(__server_id_pool, object_id - SERVER_IDS_START) == -1)
      return -1;

  } else { // client id
    bit = xdwl_bitmap_get(__server_id_pool, object_id);

    /* if bitmap is too small */
    if (bit == -1 && xdwl_error_get_code() == XDWLERR_OUTOFRANGE) {
      /* if failed to grow bitmap */
      if (xdwl_bitmap_chsize(
              __client_id_pool,
              (uint32_t)((object_id - CLIENT_IDS_START) / 8) * 8 + 8) == -1)
        return -1;

    } else if (bit == 1) {
      xdwl_error_set(XDWLERR_IDTAKEN,
                     "xdwl_object_register: failed to register server object "
                     "with id %ld. %ld is already taken",
                     object_id);
      return -1;
    }

    if (xdwl_bitmap_set(__server_id_pool, object_id - SERVER_IDS_START) == -1)
      return -1;
  }

  struct xdwl_interface *interface =
      xdwl_map_get_str(__xdwl_interfaces, object_name);
  if (interface == NULL) {
    xdwl_error_set(XDWLERR_NULLINTF,
                   "xdwl_object_register: failed to register object %s.#%d. no "
                   "interfaces found for %s object",
                   object_name, o, object_name);
    return -1;
  }

  xdwl_object object = {
      .id = o, .name = strdup(object_name), .interface = interface};

  if (xdwl_map_set(proxy->obj_reg, o, &object, sizeof(xdwl_object)) == NULL)
    return -1;

  return o;
}

int xdwl_object_unregister(xdwl_proxy *proxy, uint32_t object_id) {
  xdwl_object *object = xdwl_object_get_by_id(proxy, object_id);

  if (object) {
    if (object_id >= SERVER_IDS_START && object_id <= SERVER_IDS_END) {
      if (xdwl_bitmap_unset(__server_id_pool, object_id) == -1)
        return -1;
    } else if (object_id >= CLIENT_IDS_START && object_id <= CLIENT_IDS_END) {
      if (xdwl_bitmap_unset(__client_id_pool, object_id) == -1)
        return -1;
    }

    free(object->name);
    xdwl_map_remove(proxy->obj_reg, object_id);
    return 0;
  }

  xdwl_error_set(XDWLERR_NULLOBJ,
                 "xdwl_object_unregister: failed to unregister %ld. no object "
                 "found with id %ld",
                 object_id, object_id);
  return -1;
}

int xdwl_object_unregister_last(xdwl_proxy *proxy, const char *object_name) {
  xdwl_object *object = xdwl_object_get_by_name(proxy, object_name);

  if (object) {
    free(object->name);
    xdwl_map_remove(proxy->obj_reg, object->id);
    return 0;
  }

  xdwl_error_set(XDWLERR_NULLOBJ,
                 "xdwl_object_unregister: failed to unregister '%s'. no object "
                 "found with name '%s'",
                 object_name, object_name);
  return -1;
}

__must_check inline int xdwl_load_interfaces(const char *xml_path) {
  xmlDocPtr doc = xmlReadFile(xml_path, NULL, 0);
  if (doc == NULL) {
    xdwl_error_set(XDWLERR_NOPROTOXML, "xdwl_parse: %s not found", xml_path);
    return -1;
  }

  xmlNodePtr cur = xmlDocGetRootElement(doc);

  for (xmlNodePtr c = cur->children; c; c = c->next) {
    if (c->type == XML_ELEMENT_NODE &&
        xmlStrEqual(c->name, TO_XMLSTRING("interface"))) {
      if (xdwl_parse_interface(c, __xdwl_interfaces) == -1)
        return -1;
    }
  }
  return 0;
}

xdwl_proxy *xdwl_proxy_create() {
  int sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sock_addr = {.sun_family = AF_UNIX};

  char *display = getenv("WAYLAND_DISPLAY");
  if (display == NULL) {
    xdwl_error_set(XDWLERR_ENV, "xdwl_proxy_create: WAYLAND_DISPLAY isn't set");
    return NULL;
  }

  char *xdg_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_dir == NULL) {
    xdwl_error_set(XDWLERR_ENV, "xdwl_proxy_create: XDG_RUNTIME_DIR isn't set");
    return NULL;
  }

  size_t socket_path_len = strlen(display) + strlen(xdg_dir) + 2;

  char socket_path[socket_path_len];
  snprintf(socket_path, socket_path_len, "%s/%s", xdg_dir, display);
  strncpy(sock_addr.sun_path, socket_path, sizeof(sock_addr.sun_path) - 1);

  if (connect(sock_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("connect");
    xdwl_error_set(XDWLERR_SOCKCONN,
                   "xdwl_proxy_create: failed to connect to %s", display);
    return NULL;
  }

  if (xdwl_object_init() == -1) {
    return NULL;
  };

  xdwl_proxy *proxy = malloc(sizeof(xdwl_proxy));
  if (proxy == NULL) {
    perror("malloc");
    xdwl_error_set(XDWLERR_STD, "xdwl_proxy_create: failed to malloc() proxy");
    return NULL;
  }

  proxy->sockfd = sock_fd;

  proxy->obj_reg = xdwl_map_new(CAP);
  if (proxy->obj_reg == NULL)
    return NULL;

  proxy->buffer = malloc(CAP);
  if (proxy->buffer == NULL) {
    perror("malloc");
    xdwl_error_set(XDWLERR_STD,
                   "xdwl_proxy_create: failed to malloc() proxy buffer");
    return NULL;
  }

  __xdwl_listeners = xdwl_map_new(CAP);
  if (__xdwl_listeners == NULL)
    return NULL;

  __xdwl_interfaces = xdwl_map_new(CAP);
  if (__xdwl_interfaces == NULL)
    return NULL;

  if (xdwl_load_interfaces("/usr/share/wayland/wayland.xml") == -1)
    return NULL;

  return proxy;
}

void xdwl_proxy_destroy(xdwl_proxy *proxy) {
  if (proxy != NULL) {
    if (proxy->buffer != NULL)
      free(proxy->buffer);

    if (proxy->obj_reg != NULL)
      destroy_objects(proxy);

    close(proxy->sockfd);
    free(proxy);
  }

  if (__xdwl_interfaces != NULL)
    destroy_interfaces();

  if (__xdwl_listeners != NULL)
    xdwl_map_destroy(__xdwl_listeners);

  if (__client_id_pool != NULL)
    xdwl_bitmap_destroy(__client_id_pool);

  if (__server_id_pool != NULL)
    xdwl_bitmap_destroy(__server_id_pool);
};

int xdwl_add_listener(xdwl_proxy *proxy, const char *object_name,
                      void *interface, void *user_data) {

  xdwl_object *object = xdwl_object_get_by_name(proxy, object_name);
  if (!object) {
    xdwl_error_set(
        XDWLERR_NULLOBJ,
        "xdwl_add_listener: no registered objects found with name %s",
        object_name);
    return -1;
  }

  struct xdwl_listener listener = {.user_data = user_data,
                                   .interface = interface};
  size_t object_id = object->id;
  if (xdwl_map_set(__xdwl_listeners, object_id, &listener,
                   sizeof(struct xdwl_listener)) == NULL)
    return -1;

  return 0;
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

int xdwl_send_request(xdwl_proxy *proxy, char *object_name, size_t method_id,
                      size_t arg_count, ...) {
  va_list args;
  const xdwl_object *object = xdwl_object_get_by_name(proxy, object_name);
  if (!object) {
    xdwl_error_set(
        XDWLERR_NULLOBJ,
        "xdwl_send_request: no registered objects found with name %s",
        object_name);
    return -1;
  }
  size_t object_id = object->id;

  struct xdwl_method *request =
      xdwl_list_get(object->interface->requests, method_id);

  if (request == NULL) {
    xdwl_error_set(XDWLERR_NULLREQ,
                   "xdwl_send_request: no request found for %s#%ld",
                   object->name, method_id);
    return -1;
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
  fflush(stdout);
#endif

  size_t message_size =
      HEADER_SIZE +
      xdwl_calculate_body_size(request_args, arg_count, request_signature);
  size_t offset = 0;

  xdwl_buf_write_u32(proxy->buffer, &offset, object_id);
  xdwl_buf_write_u16(proxy->buffer, &offset, method_id);
  xdwl_buf_write_u16(proxy->buffer, &offset, message_size);
  xdwl_write_args(proxy->buffer, &offset, request_args, arg_count,
                  request_signature);

#ifdef DEBUG
  printf("\nTO SERVER\n");
  for (size_t i = 0; i < message_size; i++) {
    char c = proxy->buffer[i];
    printf("%02X ", c);
  }
  printf("\nTO SERVER\n");
  fflush(stdout);
#endif
  int n = xdwl_sock_send(proxy, message_size, fd);
  if (n < 0) {
    xdwl_error_set(XDWLERR_SOCKSEND,
                   "xdwl_send_request: failed to send message");
    return -1;
  }

  return 0;
}

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

int xdwl_recv_events(xdwl_proxy *proxy, xdwl_list *messages_list) {
  int fd = 0;
  int n = xdwl_sock_recv(proxy, &fd);

  if (n <= 0) {
    xdwl_error_set(XDWLERR_SOCKRECV, "xdwl_recv_events: server is gone");
    return -1;
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

    size_t object_id = xdwl_buf_read_u32(proxy->buffer, &_offset);
    uint16_t method_id = xdwl_buf_read_u16(proxy->buffer, &_offset);
    uint16_t message_size = xdwl_buf_read_u16(proxy->buffer, &_offset);

    xdwl_raw_message raw_message = {
        .object_id = object_id,
        .method_id = method_id,
        .body_length = message_size - HEADER_SIZE,
        .fd = fd,
    };
    memcpy(raw_message.body, proxy->buffer + _offset, raw_message.body_length);

    xdwl_object *object = xdwl_object_get_by_id(proxy, object_id);
    if (object == NULL) {
      xdwl_error_set(XDWLERR_NULLOBJ,
                     "xdwl_recv_events: no registered objects found with id %d",
                     object_id);
      return -1;
    }

    if (xdwl_list_push(messages_list, &raw_message,
                       sizeof(struct xdwl_raw_message)) == NULL)
      return -1;
    offset += message_size;
  }

  return 0;
};

int xdwl_roundtrip(xdwl_proxy *proxy) {
  size_t callback = xdwl_object_register(proxy, 0, "wl_callback");

  if (xdwl_display_sync(proxy, callback) == -1)
    return -1;

  uint8_t loop = 1;

  while (loop) {
    xdwl_list *raw_messages = xdwl_list_new();
    if (xdwl_recv_events(proxy, raw_messages) == -1)
      return -1;

    size_t msgc = xdwl_list_len(raw_messages);

    for (size_t i = 0; i < msgc; i++) {
      xdwl_raw_message *raw_message = xdwl_list_get(raw_messages, i);
      if (xdwl_dispatch_message(proxy, raw_message) == -1)
        return -1;

      if (raw_message->object_id == callback) {
        loop = 0;
      }
    }
    xdwl_list_destroy(raw_messages);
  }
  return 0;
}

int xdwl_dispatch(xdwl_proxy *proxy) {
  xdwl_list *raw_messages = xdwl_list_new();
  if (xdwl_recv_events(proxy, raw_messages) == -1)
    return -1;

  size_t msgc = xdwl_list_len(raw_messages);

  for (size_t i = 0; i < msgc; i++) {
    xdwl_raw_message *raw_message = xdwl_list_get(raw_messages, i);

    if (xdwl_dispatch_message(proxy, raw_message) == -1)
      return -1;
  }

  xdwl_list_destroy(raw_messages);
  return 0;
};
