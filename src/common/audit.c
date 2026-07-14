#include "anything/audit.h"

#include "anything/json.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const char *k_audit_contract_events[] = {
    "request_received",
    "preflight_denied",
    "approval_required",
    "approval_granted",
    "approval_rejected",
    "execution_started",
    "execution_finished",
    "execution_failed",
};

static int is_known_event(const char *event) {
  for (size_t i = 0; i < sizeof(k_audit_contract_events) / sizeof(k_audit_contract_events[0]); i++) {
    if (strcmp(event, k_audit_contract_events[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

static void timestamp_utc(char *out, size_t out_len) {
  time_t now = time(NULL);
  struct tm tm_utc;
  gmtime_r(&now, &tm_utc);
  strftime(out, out_len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static int audit_path_under(const char *path, const char *root) {
  size_t root_len = strlen(root);
  if (root_len == 0) {
    return 0;
  }
  if (strncmp(path, root, root_len) != 0) {
    return 0;
  }
  return path[root_len] == '\0' || path[root_len] == '/';
}

static int canonical_path_under(const char *path, const char *root) {
  char real_path[ANYTHING_MAX_PATH_LEN];
  char real_root[ANYTHING_MAX_PATH_LEN];
  if (realpath(path, real_path) == NULL || realpath(root, real_root) == NULL) {
    return audit_path_under(path, root);
  }
  return audit_path_under(real_path, real_root);
}

static int open_audit_fd(const char *path, char *error, size_t error_len) {
  int fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
  if (fd < 0) {
    if (errno == ELOOP) {
      snprintf(error, error_len, "audit log must not be a symlink");
    } else {
      snprintf(error, error_len, "audit log is not writable");
    }
    return -1;
  }
  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
    snprintf(error, error_len, "audit log must be a regular file");
    close(fd);
    return -1;
  }
  return fd;
}

int anything_audit_validate_startup(const anything_config *config, char *error, size_t error_len) {
  if (config->audit_log_path[0] == '\0') {
    snprintf(error, error_len, "audit log path is required");
    return -1;
  }
  for (size_t i = 0; i < config->path_count; i++) {
    if (config->paths[i].writable && canonical_path_under(config->audit_log_path, config->paths[i].path)) {
      snprintf(error, error_len, "audit log path is inside an agent writable allowlist");
      return -1;
    }
  }

  char parent[ANYTHING_MAX_PATH_LEN];
  snprintf(parent, sizeof(parent), "%s", config->audit_log_path);
  char *slash = strrchr(parent, '/');
  if (slash == NULL || slash == parent) {
    snprintf(error, error_len, "audit log path must include an existing parent directory");
    return -1;
  }
  *slash = '\0';
  struct stat st;
  if (stat(parent, &st) != 0 || !S_ISDIR(st.st_mode)) {
    snprintf(error, error_len, "audit log parent directory is not available");
    return -1;
  }

  int fd = open_audit_fd(config->audit_log_path, error, error_len);
  if (fd < 0) {
    return -1;
  }
  close(fd);
  return 0;
}

int anything_audit_write_event(const anything_config *config, const char *event, const char *request_id, const char *session_id, anything_identity caller, const anything_identity *approver, const char *method, const char *risk, const char *decision, const char *error_kind, const char *params_summary, long duration_ms) {
  (void)is_known_event(event);
  char audit_error[ANYTHING_AUDIT_MAX_ERROR];
  int fd = open_audit_fd(config->audit_log_path, audit_error, sizeof(audit_error));
  if (fd < 0) {
    return -1;
  }

  char ts[32];
  char escaped_event[64];
  char escaped_request_id[128];
  char escaped_session_id[128];
  char escaped_method[128];
  char escaped_risk[64];
  char escaped_decision[64];
  char escaped_error_kind[128];
  char escaped_params_summary[512];
  if (anything_json_escape_string(event, escaped_event, sizeof(escaped_event)) != 0 ||
      anything_json_escape_string(request_id != NULL ? request_id : "", escaped_request_id, sizeof(escaped_request_id)) != 0 ||
      anything_json_escape_string(session_id != NULL ? session_id : "", escaped_session_id, sizeof(escaped_session_id)) != 0 ||
      anything_json_escape_string(method != NULL ? method : "", escaped_method, sizeof(escaped_method)) != 0 ||
      anything_json_escape_string(risk != NULL ? risk : "", escaped_risk, sizeof(escaped_risk)) != 0 ||
      anything_json_escape_string(decision != NULL ? decision : "", escaped_decision, sizeof(escaped_decision)) != 0 ||
      anything_json_escape_string(error_kind != NULL ? error_kind : "", escaped_error_kind, sizeof(escaped_error_kind)) != 0 ||
      anything_json_escape_string(params_summary != NULL ? params_summary : "", escaped_params_summary, sizeof(escaped_params_summary)) != 0) {
    close(fd);
    return -1;
  }
  timestamp_utc(ts, sizeof(ts));
  int failed = dprintf(fd,
                       "{\"timestamp\":\"%s\",\"event\":\"%s\",\"request_id\":\"%s\",\"session_id\":\"%s\","
                       "\"caller\":{\"uid\":%ld,\"gid\":%ld,\"pid\":%ld},",
                       ts, escaped_event, escaped_request_id, escaped_session_id,
                       (long)caller.uid, (long)caller.gid, (long)caller.pid) < 0;
  if (approver != NULL) {
    failed = failed || dprintf(fd, "\"approver\":{\"uid\":%ld,\"gid\":%ld,\"pid\":%ld},", (long)approver->uid, (long)approver->gid, (long)approver->pid) < 0;
  }
  failed = failed || dprintf(fd,
                             "\"method\":\"%s\",\"risk\":\"%s\",\"decision\":\"%s\",\"error_kind\":\"%s\","
                             "\"duration_ms\":%ld,\"params_summary\":\"%s\"}\n",
                             escaped_method, escaped_risk, escaped_decision,
                             escaped_error_kind, duration_ms, escaped_params_summary) < 0;

  if (close(fd) != 0) {
    failed = 1;
  }
  return failed ? -1 : 0;
}
