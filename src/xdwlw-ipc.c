#include "xdwlw-error.h"
#include "xdwlw-types.h"

#include <linux/limits.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MAX_SOCK_BUFFER_SIZE 512

static void ipc_get_sock_path(char *path) {
  char *dir = getenv("XDG_RUNTIME_DIR");
  if (!dir) {
    dir = "/tmp";
  }
  snprintf(path, 108, "%s/xdwlw-%d.sock", dir, getuid());
}

uint32_t buf_read_u32(void *buffer, size_t *offset) {
  uint32_t value = *(uint32_t *)(buffer + *offset);
  *offset += sizeof(value);

  return value;
}

uint8_t buf_read_u8(void *buffer, size_t *offset) {
  uint8_t value = *(uint8_t *)(buffer + *offset);
  *offset += sizeof(value);

  return value;
}

char *buf_read_string(void *buffer, size_t *offset) {
  uint16_t string_length = *(uint16_t *)(buffer + *offset);
  *offset += sizeof(string_length);

  char *string = buffer + *offset;
  *offset += string_length;

  return string;
}

void buf_write_u32(void *buffer, size_t *offset, uint32_t n) {
  *(uint32_t *)(buffer + *offset) = n;
  *offset += sizeof(n);
}

void buf_write_u8(void *buffer, size_t *offset, uint8_t n) {
  *(uint8_t *)(buffer + *offset) = n;
  *offset += sizeof(n);
}

void buf_write_string(void *buffer, size_t *offset, const char *string) {
  uint16_t string_length = strlen(string) + 1;
  *(uint16_t *)(buffer + *offset) = string_length;
  *offset += sizeof(uint16_t);

  memcpy((char *)(buffer + *offset), string, string_length);

  *offset += string_length;
}

int ipc_client_connect() {
  struct sockaddr_un sock_addr;
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  ipc_get_sock_path(sock_addr.sun_path);

  if (connect(fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("connect");
    xdwlw_error_set(XDWLWE_DCONN, "xdwlw: failed to connect to daemon");
    return -1;
  }

  return fd;
}

struct ipc_message *ipc_client_recv(int sfd) {
  char buffer[MAX_SOCK_BUFFER_SIZE];
  memset(buffer, 0, MAX_SOCK_BUFFER_SIZE);

  size_t offset = 0;
  struct ipc_message *msg;

  if (recv(sfd, buffer, sizeof(buffer), 0) == 0)
    return NULL;

  msg = malloc(sizeof(struct ipc_message));
  msg->type = buf_read_u8(buffer, &offset);

  switch (msg->type) {
  case IPC_SERVER_ERR:
    snprintf(msg->error.msg, sizeof(msg->error.msg), "%s",
             buf_read_string(buffer, &offset));
    return msg;

  default:
    return msg;
  }
}

int ipc_client_send(int sfd, struct ipc_message *msg) {
  char buffer[MAX_SOCK_BUFFER_SIZE];
  memset(buffer, 0, MAX_SOCK_BUFFER_SIZE);

  size_t offset = 0;
  buf_write_u8(buffer, &offset, msg->type);

  switch (msg->type) {
  case IPC_ACK:
  case IPC_CLIENT_KILL:
    break;

  case IPC_CLIENT_SET_IMAGE:
    buf_write_string(buffer, &offset, msg->set_image.output);
    buf_write_u8(buffer, &offset, msg->set_image.mode);
    buf_write_string(buffer, &offset, msg->set_image.path);
    break;

  case IPC_CLIENT_SET_COLOR:
    buf_write_string(buffer, &offset, msg->set_color.output);
    buf_write_u32(buffer, &offset, msg->set_color.color);
    break;

  default:
    buf_write_u8(buffer, &offset, IPC_UNKNOWN);
    break;
  }

  return send(sfd, buffer, offset, 0);
}

int ipc_server_start() {
  struct sockaddr_un sock_addr;

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1) {
    perror("socket");
    return -1;
  }

  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  ipc_get_sock_path(sock_addr.sun_path);

  if (bind(fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) == -1) {
    perror("bind");
    return -1;
  }

  if (listen(fd, 10) == -1)
    return -1;

  return fd;
}

struct ipc_message *ipc_server_listen(int sfd, int *cfd) {
  if (sfd == 0 || cfd == NULL) {
    return NULL;
  }

  struct sockaddr_un client_addr;
  socklen_t client_addr_size;

  client_addr_size = sizeof(client_addr);
  *cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_addr_size);
  if (*cfd == -1) {
    perror("accept");
    return NULL;
  }

  char buffer[MAX_SOCK_BUFFER_SIZE];
  memset(buffer, 0, MAX_SOCK_BUFFER_SIZE);

  size_t offset = 0;
  recv(*cfd, buffer, sizeof(buffer), 0);

  struct ipc_message *msg = malloc(sizeof(struct ipc_message));
  if (!msg) {
    perror("malloc");
    return NULL;
  }

  msg->type = buf_read_u8(buffer, &offset);

  switch (msg->type) {
  case IPC_ACK:
  case IPC_CLIENT_KILL:
    return msg;

  case IPC_CLIENT_SET_IMAGE:
    msg->set_image.output = strdup(buf_read_string(buffer, &offset));
    msg->set_image.mode = buf_read_u8(buffer, &offset);
    msg->set_image.path = strdup(buf_read_string(buffer, &offset));
    return msg;

  case IPC_CLIENT_SET_COLOR:
    msg->set_color.output = strdup(buf_read_string(buffer, &offset));
    msg->set_color.color = buf_read_u32(buffer, &offset);
    return msg;

  default:
    msg->type = IPC_UNKNOWN;
    return msg;
  }
}

int ipc_server_send(int cfd, struct ipc_message *msg) {
  char buffer[MAX_SOCK_BUFFER_SIZE];
  memset(buffer, 0, MAX_SOCK_BUFFER_SIZE);
  size_t offset = 0;

  if (cfd == 0)
    return 0;

  buf_write_u8(buffer, &offset, msg->type);

  switch (msg->type) {
  case IPC_SERVER_ERR:
    buf_write_string(buffer, &offset, msg->error.msg);
    break;

  default:
    break;
  }

  int n = send(cfd, buffer, offset, 0);
  close(cfd);

  return n;
}

int ipc_server_close(int sfd) {
  if (sfd > 0) {
    if (close(sfd) == -1) {
      perror("close");
      return -1;
    }

    char sock_path[PATH_MAX];
    ipc_get_sock_path(sock_path);

    if (unlink(sock_path) == -1) {
      perror("unlink");
      return -1;
    };
  }
  return 0;
}

void ipc_message_free(struct ipc_message *msg) {
  switch (msg->type) {
  case IPC_CLIENT_SET_IMAGE:
    free((char *)msg->set_image.output);
    free((char *)msg->set_image.path);
    break;

  case IPC_CLIENT_SET_COLOR:
    free((char *)msg->set_color.output);
    break;

  default:
    break;
  }
}
