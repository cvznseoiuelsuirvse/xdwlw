#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "xdwlw-common.h"
#include "xdwlw-error.h"
#include "xdwlw-ipc.h"
#include "xdwlw-types.h"

#include <getopt.h>
#include <time.h>

void print_help() {
  printf("usage: xdwlw [-h] [-0] [-i IMAGE] [-m {F, C}] [-c COLOR] "
         "[-o OUTPUT]\n");
  printf("  -h, --help    show this message\n");
  printf("  -0, --kill    kill daemon\n");
  printf("  -c, --color   color to set as wallpaper\n");
  printf("  -i, --image   image to set as wallpaper\n");
  printf("  -m, --mode    how to display (fit) wallpaper.\n"
         "                  available modes: [F]it, [C]enter\n");
  printf("  -o, --output  which outputs to set wallpaper on.\n"
         "                  default is all\n");
}

struct ipc_message *send_and_recv(int fd, struct ipc_message *msg) {
  int n;
  if ((n = ipc_client_send(fd, msg)) == 0) {
    xdwlw_error_set(XDWLWE_DRECV,
                    "send_and_recv: failed to send message to daemon");
    close(fd);
    return NULL;
  };

  struct ipc_message *resp;
  if ((resp = ipc_client_recv(fd)) == NULL) {
    xdwlw_error_set(XDWLWE_DSEND,
                    "send_and_recv: failed to get response from daemon");
    close(fd);
    return NULL;
  };

  return resp;
}

void wait(size_t ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1E6;
  nanosleep(&ts, NULL);
}

int main(int argc, char **argv) {

  if (argc < 2) {
    print_help();
    return 1;
  }

  struct ipc_message msg;
  struct ipc_message *resp;

  uint8_t kill = 0;
  char *output = NULL;

  char image_path[PATH_MAX];
  enum image_modes image_mode = 0;

  int color = -1;

  int opt;
  int option_index;
  static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                         {"kill", no_argument, 0, '0'},
                                         {"image", optional_argument, 0, 'i'},
                                         {"output", optional_argument, 0, 'o'},
                                         {"color", optional_argument, 0, 'c'},
                                         {"mode", optional_argument, 0, 'm'},
                                         {0, 0, 0, 0}};

  while ((opt = getopt_long(argc, argv, "h0i:o:c:m:", long_options,
                            &option_index)) != -1) {

    switch (opt) {
    case 'h':
      print_help();
      return 0;

    case '0':
      kill = 1;
      break;

    case 'i':
      if (strlen(optarg) > 64) {
        xdwlw_error_set(XDWLWE_LONGSTR,
                        "xdwlw: image argument is >64 characters long");
        xdwlw_error_print();
        return 1;
      }

      if (!realpath(optarg, image_path)) {
        if (errno == ENOENT) {
          xdwlw_error_set(XDWLWE_NOIMG, "xdwlw: specified image not found");
          xdwlw_error_print();
        } else {
          perror("realpath");
        }
        return 1;
      }

      break;

    case 'c':
      color = (uint32_t)strtol(optarg, NULL, 16);
      break;

    case 'o':
      if (strlen(optarg) > 64) {
        xdwlw_error_set(XDWLWE_LONGSTR,
                        "xdwlw: output argument is >64 characters long");
        xdwlw_error_print();
        return 1;
      }

      output = optarg;
      break;

    case 'm':
      image_mode = (char)optarg[0];
      break;

    case '?':
      break;
    }
  }

  if (output == NULL) {
    output = "all";
  }

  if (image_path[0] != '\0' && image_mode != 0) {
    msg.type = IPC_CLIENT_SET_IMAGE;
    msg.set_image.output = output;
    msg.set_image.mode = image_mode;
    msg.set_image.path = image_path;

  } else if (color != -1) {
    msg.type = IPC_CLIENT_SET_COLOR;
    msg.set_color.output = output;
    msg.set_color.color = color;

  } else if (kill > 0) {
    msg.type = IPC_CLIENT_KILL;
  } else {
    print_help();
    return 1;
  }

  int fd = ipc_client_connect();
  if (fd < 0) {
    xdwlw_error_set(XDWLWE_DCONN, "xdwlw: failed to connect to daemon");
    xdwlw_error_print();
    return 1;
  }

  resp = send_and_recv(fd, &msg);
  if (resp == NULL) {
    xdwlw_error_print();
    return 1;
  }

  switch (resp->type) {
  case IPC_SERVER_ERR:
    fprintf(stderr, "xdwlwd: %s", resp->error.msg);
    return 1;

  case IPC_ACK:
    xdwlw_log("info", "done");
    break;

  default:
    xdwlw_log("warn",
              "received message back from daemon with unknown type"
              "retrying",
              resp->type);
    break;
  }

  ipc_message_free(resp);
  close(fd);

  return 0;
}
