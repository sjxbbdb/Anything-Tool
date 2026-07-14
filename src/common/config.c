#include "anything/config.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_error(char *error, size_t error_len, const char *message) {
  if (error != NULL && error_len > 0) {
    snprintf(error, error_len, "%s", message);
  }
}

static char *trim(char *text) {
  while (isspace((unsigned char)*text)) {
    text++;
  }
  char *end = text + strlen(text);
  while (end > text && isspace((unsigned char)end[-1])) {
    *--end = '\0';
  }
  return text;
}

static int parse_quoted(const char *value, char *out, size_t out_len) {
  const char *start = strchr(value, '"');
  if (start == NULL) {
    return -1;
  }
  start++;
  const char *end = strchr(start, '"');
  if (end == NULL || (size_t)(end - start) >= out_len) {
    return -1;
  }
  memcpy(out, start, (size_t)(end - start));
  out[end - start] = '\0';
  return 0;
}

static int parse_long_array(const char *value, long *out, size_t max_count, size_t *out_count) {
  const char *p = strchr(value, '[');
  if (p == NULL) {
    return -1;
  }
  p++;
  *out_count = 0;
  while (*p != '\0') {
    p = trim((char *)p);
    if (*p == ']') {
      return 0;
    }
    if (*out_count >= max_count) {
      return -1;
    }
    char *end = NULL;
    long parsed = strtol(p, &end, 10);
    if (end == p) {
      return -1;
    }
    out[(*out_count)++] = parsed;
    p = end;
    while (isspace((unsigned char)*p)) {
      p++;
    }
    if (*p == ',') {
      p++;
      continue;
    }
    if (*p == ']') {
      return 0;
    }
    return -1;
  }
  return -1;
}

static int path_is_under(const char *path, const char *root) {
  size_t root_len = strlen(root);
  if (root_len == 0) {
    return 0;
  }
  if (strncmp(path, root, root_len) != 0) {
    return 0;
  }
  return path[root_len] == '\0' || path[root_len] == '/';
}

void anything_config_init(anything_config *config) {
  memset(config, 0, sizeof(*config));
  snprintf(config->tool_socket_path, sizeof(config->tool_socket_path), "/tmp/anythingd-tool.sock");
  snprintf(config->admin_socket_path, sizeof(config->admin_socket_path), "/tmp/anythingd-admin.sock");
  snprintf(config->audit_log_path, sizeof(config->audit_log_path), "/tmp/anything-audit.jsonl");
  config->capability_sys_read = 0;
  config->max_request_bytes = 1048576;
  config->max_output_bytes = 1048576;
  config->approval_ttl_seconds = 300;
  config->read_timeout_ms = 2000;
  config->require_admin_allowlist = 0;
}

int anything_config_validate(const anything_config *config, char *error, size_t error_len) {
  if (config->tool_socket_path[0] == '\0') {
    set_error(error, error_len, "daemon.tool_socket_path is required");
    return -1;
  }
  if (config->admin_socket_path[0] == '\0') {
    set_error(error, error_len, "daemon.admin_socket_path is required");
    return -1;
  }
  if (strcmp(config->tool_socket_path, config->admin_socket_path) == 0) {
    set_error(error, error_len, "tool and admin socket paths must differ");
    return -1;
  }
  if (config->audit_log_path[0] == '\0') {
    set_error(error, error_len, "daemon.audit_log is required");
    return -1;
  }
  if (config->max_request_bytes == 0 || config->max_request_bytes > 1048576) {
    set_error(error, error_len, "limits.max_request_bytes must be 1..1048576");
    return -1;
  }
  if (config->read_timeout_ms <= 0 || config->read_timeout_ms > 60000) {
    set_error(error, error_len, "limits.read_timeout_ms must be 1..60000");
    return -1;
  }
  if (config->require_admin_allowlist && config->admin_allowed_uid_count == 0 && config->admin_allowed_gid_count == 0) {
    set_error(error, error_len, "admin allowlist is required but empty");
    return -1;
  }
  for (size_t i = 0; i < config->path_count; i++) {
    if (config->paths[i].writable && path_is_under(config->audit_log_path, config->paths[i].path)) {
      set_error(error, error_len, "audit log path is inside an agent writable allowlist");
      return -1;
    }
  }
  return 0;
}

int anything_config_load(const char *path, anything_config *config, char *error, size_t error_len) {
  anything_config_init(config);
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    set_error(error, error_len, "failed to open config file");
    return -1;
  }

  char line[512];
  int in_path_rule = 0;
  while (fgets(line, sizeof(line), file) != NULL) {
    char *hash = strchr(line, '#');
    if (hash != NULL) {
      *hash = '\0';
    }
    char *entry = trim(line);
    if (entry[0] == '\0') {
      continue;
    }
    if (strcmp(entry, "[[paths]]") == 0) {
      if (config->path_count >= ANYTHING_MAX_PATH_RULES) {
        fclose(file);
        set_error(error, error_len, "too many path rules");
        return -1;
      }
      in_path_rule = 1;
      config->path_count++;
      continue;
    }

    char *equals = strchr(entry, '=');
    if (equals == NULL) {
      continue;
    }
    *equals = '\0';
    char *key = trim(entry);
    char *value = trim(equals + 1);

    if (strcmp(key, "tool_socket_path") == 0 || strcmp(key, "socket_path") == 0) {
      if (parse_quoted(value, config->tool_socket_path, sizeof(config->tool_socket_path)) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid tool socket path");
        return -1;
      }
    } else if (strcmp(key, "admin_socket_path") == 0) {
      if (parse_quoted(value, config->admin_socket_path, sizeof(config->admin_socket_path)) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid admin socket path");
        return -1;
      }
    } else if (strcmp(key, "audit_log") == 0) {
      if (parse_quoted(value, config->audit_log_path, sizeof(config->audit_log_path)) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid audit log path");
        return -1;
      }
    } else if (strcmp(key, "sys_read") == 0) {
      config->capability_sys_read = strncmp(value, "true", 4) == 0;
    } else if (strcmp(key, "max_request_bytes") == 0) {
      config->max_request_bytes = (size_t)strtoull(value, NULL, 10);
    } else if (strcmp(key, "max_output_bytes") == 0) {
      config->max_output_bytes = (size_t)strtoull(value, NULL, 10);
    } else if (strcmp(key, "approval_ttl_seconds") == 0) {
      config->approval_ttl_seconds = atoi(value);
    } else if (strcmp(key, "read_timeout_ms") == 0) {
      config->read_timeout_ms = atoi(value);
    } else if (strcmp(key, "allowed_uids") == 0) {
      if (parse_long_array(value, config->admin_allowed_uids, ANYTHING_MAX_ADMIN_IDS, &config->admin_allowed_uid_count) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid admin allowed_uids");
        return -1;
      }
    } else if (strcmp(key, "allowed_gids") == 0) {
      if (parse_long_array(value, config->admin_allowed_gids, ANYTHING_MAX_ADMIN_IDS, &config->admin_allowed_gid_count) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid admin allowed_gids");
        return -1;
      }
    } else if (strcmp(key, "require_admin_allowlist") == 0) {
      config->require_admin_allowlist = strncmp(value, "true", 4) == 0;
    } else if (in_path_rule && config->path_count > 0 && strcmp(key, "path") == 0) {
      if (parse_quoted(value, config->paths[config->path_count - 1].path, sizeof(config->paths[0].path)) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid path rule");
        return -1;
      }
    } else if (in_path_rule && config->path_count > 0 && strcmp(key, "mode") == 0) {
      char mode[32];
      if (parse_quoted(value, mode, sizeof(mode)) != 0) {
        fclose(file);
        set_error(error, error_len, "invalid path mode");
        return -1;
      }
      config->paths[config->path_count - 1].writable = strstr(mode, "write") != NULL;
    }
  }

  fclose(file);
  return anything_config_validate(config, error, error_len);
}

int anything_config_identity_is_admin(const anything_config *config, anything_identity identity) {
  if (!config->require_admin_allowlist) {
    return 1;
  }
  for (size_t i = 0; i < config->admin_allowed_uid_count; i++) {
    if (config->admin_allowed_uids[i] == (long)identity.uid) {
      return 1;
    }
  }
  for (size_t i = 0; i < config->admin_allowed_gid_count; i++) {
    if (config->admin_allowed_gids[i] == (long)identity.gid) {
      return 1;
    }
  }
  return 0;
}
