#include "anything/config.h"
#include "anything/transport.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int connect_socket(const char *path) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    return -1;
  }
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    close(fd);
    return -1;
  }
  return fd;
}

static void usage(void) {
  fprintf(stderr,
          "anythingctl v0.1.0\n"
          "Usage:\n"
          "  anythingctl --socket PATH raw JSON\n"
          "  anythingctl --socket PATH sys-info SESSION\n"
          "  anythingctl --admin-socket PATH pending\n"
          "  anythingctl --admin-socket PATH approve REQUEST_ID\n"
          "  anythingctl --admin-socket PATH reject REQUEST_ID\n"
          "  anythingctl --admin-socket PATH execute REQUEST_ID\n");
}

static void make_request(const char *command, const char *arg, char *out, size_t out_len) {
  if (strcmp(command, "sys-info") == 0) {
    snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"sys.info\",\"params\":{\"session_id\":\"%s\"}}\n", arg);
  } else if (strcmp(command, "pending") == 0) {
    snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"approval.list\",\"params\":{}}\n");
  } else if (strcmp(command, "approve") == 0) {
    snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"approval.approve\",\"params\":{\"request_id\":\"%s\"}}\n", arg);
  } else if (strcmp(command, "reject") == 0) {
    snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"approval.reject\",\"params\":{\"request_id\":\"%s\"}}\n", arg);
  } else if (strcmp(command, "execute") == 0) {
    snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"approval.execute\",\"params\":{\"request_id\":\"%s\"}}\n", arg);
  } else {
    out[0] = '\0';
  }
}

int main(int argc, char **argv) {
  if (argc < 4) {
    usage();
    return 2;
  }

  const char *socket_path = NULL;
  const char *command = NULL;
  const char *arg = "";
  if (strcmp(argv[1], "--socket") == 0 || strcmp(argv[1], "--admin-socket") == 0) {
    socket_path = argv[2];
    command = argv[3];
    if (argc > 4) {
      arg = argv[4];
    }
  } else {
    usage();
    return 2;
  }

  char request[2048];
  if (strcmp(command, "raw") == 0) {
    if (argc < 5) {
      usage();
      return 2;
    }
    snprintf(request, sizeof(request), "%s\n", argv[4]);
  } else {
    make_request(command, arg, request, sizeof(request));
  }
  if (request[0] == '\0') {
    usage();
    return 2;
  }

  int fd = connect_socket(socket_path);
  if (fd < 0) {
    fprintf(stderr, "connect failed: %s\n", strerror(errno));
    return 1;
  }
  anything_transport_write_response(fd, request);
  shutdown(fd, SHUT_WR);

  char response[4096];
  ssize_t n = read(fd, response, sizeof(response) - 1);
  if (n < 0) {
    fprintf(stderr, "read failed: %s\n", strerror(errno));
    close(fd);
    return 1;
  }
  response[n] = '\0';
  fputs(response, stdout);
  close(fd);
  return 0;
}
