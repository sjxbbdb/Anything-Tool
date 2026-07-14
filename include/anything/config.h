#ifndef ANYTHING_CONFIG_H
#define ANYTHING_CONFIG_H

#include <stddef.h>

#include "anything/identity.h"

#define ANYTHING_VERSION "v0.1.2"
#define ANYTHING_MAX_PATH_RULES 16
#define ANYTHING_MAX_PATH_LEN 256
#define ANYTHING_MAX_ERROR_LEN 256
#define ANYTHING_MAX_ADMIN_IDS 16

typedef struct anything_path_rule {
  char path[ANYTHING_MAX_PATH_LEN];
  int writable;
} anything_path_rule;

typedef struct anything_config {
  char tool_socket_path[ANYTHING_MAX_PATH_LEN];
  char admin_socket_path[ANYTHING_MAX_PATH_LEN];
  char audit_log_path[ANYTHING_MAX_PATH_LEN];
  int capability_sys_read;
  size_t max_request_bytes;
  size_t max_output_bytes;
  int approval_ttl_seconds;
  int read_timeout_ms;
  long admin_allowed_uids[ANYTHING_MAX_ADMIN_IDS];
  long admin_allowed_gids[ANYTHING_MAX_ADMIN_IDS];
  size_t admin_allowed_uid_count;
  size_t admin_allowed_gid_count;
  int require_admin_allowlist;
  anything_path_rule paths[ANYTHING_MAX_PATH_RULES];
  size_t path_count;
} anything_config;

void anything_config_init(anything_config *config);
int anything_config_load(const char *path, anything_config *config, char *error, size_t error_len);
int anything_config_load_with_options(const char *path, anything_config *config, int allow_dev_insecure_admin, char *error, size_t error_len);
int anything_config_validate(const anything_config *config, char *error, size_t error_len);
int anything_config_identity_is_admin(const anything_config *config, anything_identity identity);

#endif
