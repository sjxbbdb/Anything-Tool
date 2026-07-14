#include "anything/transport.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

int anything_transport_listen(const char *path, int mode, char *error, size_t error_len) {
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) {
    snprintf(error, error_len, "socket failed: %s", strerror(errno));
    return -1;
  }

  unlink(path);
  struct sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  if (strlen(path) >= sizeof(addr.sun_path)) {
    snprintf(error, error_len, "socket path too long");
    close(fd);
    return -1;
  }
  snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
  if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
    snprintf(error, error_len, "bind failed: %s", strerror(errno));
    close(fd);
    return -1;
  }
  chmod(path, (mode_t)mode);
  if (listen(fd, 16) != 0) {
    snprintf(error, error_len, "listen failed: %s", strerror(errno));
    close(fd);
    return -1;
  }
  return fd;
}

int anything_transport_peer_identity(int fd, anything_identity *identity, char *error, size_t error_len) {
  struct ucred cred;
  socklen_t len = sizeof(cred);
  if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0) {
    snprintf(error, error_len, "SO_PEERCRED failed: %s", strerror(errno));
    return -1;
  }
  identity->uid = cred.uid;
  identity->gid = cred.gid;
  identity->pid = cred.pid;
  return 0;
}

int anything_transport_set_read_timeout(int fd, int timeout_ms, char *error, size_t error_len) {
  struct timeval timeout;
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
    snprintf(error, error_len, "SO_RCVTIMEO failed: %s", strerror(errno));
    return -1;
  }
  return 0;
}

static long elapsed_ms_since(const struct timespec *start) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  long sec = (long)(now.tv_sec - start->tv_sec);
  long nsec = (long)(now.tv_nsec - start->tv_nsec);
  return sec * 1000L + nsec / 1000000L;
}

int anything_transport_read_request(int fd, char *buffer, size_t max_bytes, int deadline_ms, size_t *out_len, char *error, size_t error_len) {
  struct timespec started;
  clock_gettime(CLOCK_MONOTONIC, &started);
  size_t total = 0;
  while (total < max_bytes) {
    if (elapsed_ms_since(&started) > deadline_ms) {
      snprintf(error, error_len, "request deadline exceeded");
      return -1;
    }
    ssize_t n = read(fd, buffer + total, max_bytes - total);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      snprintf(error, error_len, "read failed: %s", strerror(errno));
      return -1;
    }
    if (n == 0) {
      break;
    }
    total += (size_t)n;
    if (memchr(buffer, '\n', total) != NULL) {
      break;
    }
  }
  if (elapsed_ms_since(&started) > deadline_ms) {
    snprintf(error, error_len, "request deadline exceeded");
    return -1;
  }
  if (total >= max_bytes) {
    snprintf(error, error_len, "request exceeds configured max_request_bytes");
    return -1;
  }
  buffer[total] = '\0';
  *out_len = total;
  return 0;
}

int anything_transport_write_response(int fd, const char *response) {
  size_t len = strlen(response);
  size_t written = 0;
  while (written < len) {
    ssize_t n = write(fd, response + written, len - written);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    written += (size_t)n;
  }
  return 0;
}

void anything_transport_cleanup_socket(const char *path) {
  unlink(path);
}

/* The daemon intentionally exposes a tool socket and a separate admin socket.
 * Each accepted connection must pass through SO_PEERCRED before routing. */
