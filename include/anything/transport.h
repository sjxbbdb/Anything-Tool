#ifndef ANYTHING_TRANSPORT_H
#define ANYTHING_TRANSPORT_H

#include <stddef.h>

#include "anything/config.h"
#include "anything/identity.h"

typedef enum anything_socket_plane {
  ANYTHING_SOCKET_TOOL = 0,
  ANYTHING_SOCKET_ADMIN = 1
} anything_socket_plane;

int anything_transport_listen(const char *path, int mode, char *error, size_t error_len);
int anything_transport_peer_identity(int fd, anything_identity *identity, char *error, size_t error_len);
int anything_transport_set_read_timeout(int fd, int timeout_ms, char *error, size_t error_len);
int anything_transport_read_request(int fd, char *buffer, size_t max_bytes, int deadline_ms, size_t *out_len, char *error, size_t error_len);
int anything_transport_write_response(int fd, const char *response);
void anything_transport_cleanup_socket(const char *path);

#endif
