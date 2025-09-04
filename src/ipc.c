#include "structs.h"

#include <linux/limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define MAX_SOCK_BUFFER_SIZE 512
#define PADDED4(n) ((n + 4) & ~3)

static void ipc_get_sock_path(char *path) {
  char *dir = getenv("XDG_RUNTIME_DIR");
  if (!dir) {
    dir = "/tmp";
  }
  snprintf(path, 108, "%s/xdwlw-%d.sock", dir, getuid());
}

static uint32_t buf_read_u32(void *buffer, size_t *offset) {
  uint32_t value = *(uint32_t *)(buffer + *offset);
  *offset += sizeof(value);

  return value;
}

static uint16_t buf_read_u16(void *buffer, size_t *offset) {
  uint16_t value = *(uint16_t *)(buffer + *offset);
  *offset += sizeof(value);

  return value;
}

static uint8_t buf_read_u8(void *buffer, size_t *offset) {
  uint8_t value = *(uint8_t *)(buffer + *offset);
  *offset += sizeof(value);

  return value;
}

static char *buf_read_string(void *buffer, size_t *offset) {
  uint16_t string_length = *(uint16_t *)(buffer + *offset);
  *offset += sizeof(string_length);

  char *string = (char *)(buffer + *offset);
  return string;
}

void buf_write_u32(void *buffer, size_t *offset, uint32_t n) {
  *(uint32_t *)(buffer + *offset) = n;
  *offset += sizeof(n);
}

void buf_write_u16(void *buffer, size_t *offset, uint16_t n) {
  *(uint16_t *)(buffer + *offset) = n;
  *offset += sizeof(n);
}

void buf_write_u8(void *buffer, size_t *offset, uint8_t n) {
  *(uint8_t *)(buffer + *offset) = n;
  *offset += sizeof(n);
}

void buf_write_string(void *buffer, size_t *offset, const char *string) {
  uint16_t string_length = strlen(string);
  *(uint16_t *)(buffer + *offset) = string_length + 1;
  *offset += sizeof(uint16_t);

  string_length = PADDED4(string_length);
  memcpy((char *)(buffer + *offset), string, string_length);
  *offset += string_length;
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

  case IPC_SERVER_ERR:
    free((char *)msg->error.msg);
    break;

  default:
    break;
  }
}

int ipc_client_recv() {}
int ipc_client_send() {}

int ipc_server_init() {
  struct sockaddr_un server_addr;

  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd == -1)
    return -1;

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sun_family = AF_UNIX;
  ipc_get_sock_path(server_addr.sun_path);

  if (bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    return -1;

  return fd;
}

struct ipc_message *ipc_server_listen(int sfd, int *cfd) {
  if (sfd == 0 || cfd == NULL)
    return NULL;

  struct sockaddr_un client_addr;
  socklen_t client_addr_size;
  if (listen(sfd, 10) == -1)
    return NULL;

  client_addr_size = sizeof(client_addr);
  *cfd = accept(sfd, (struct sockaddr *)&client_addr, &client_addr_size);
  if (*cfd == -1)
    return NULL;

  char buffer[MAX_SOCK_BUFFER_SIZE];
  size_t offset = 0;
  recv(*cfd, buffer, sizeof(buffer), 0);

  struct ipc_message *msg = malloc(sizeof(struct ipc_message));
  if (!msg)
    return NULL;

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
  char *buffer[MAX_SOCK_BUFFER_SIZE];
  size_t offset = 0;

  if (cfd == 0)
    return 0;

  buf_write_u8(buffer, &offset, msg->type);

  switch (msg->type) {
  case IPC_SERVER_ERR:
    buf_write_u16(buffer, &offset, msg->error.code);
    buf_write_string(buffer, &offset, msg->error.msg);
    break;

  default:
    break;
  }

  return send(cfd, buffer, offset, 0);
}

int ipc_server_close(int fd) {
  if (close(fd) == -1)
    return -1;

  char *sock_path;
  ipc_get_sock_path(sock_path);
  if (unlink(sock_path) == -1)
    return -1;

  return 0;
}
