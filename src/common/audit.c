#include "anything/audit.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

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

int anything_audit_write_event(const anything_config *config, const char *event, const char *request_id, const char *session_id, anything_identity caller, const anything_identity *approver, const char *method, const char *risk, const char *decision, const char *error_kind, const char *params_summary, long duration_ms) {
  (void)is_known_event(event);
  FILE *file = fopen(config->audit_log_path, "a");
  if (file == NULL) {
    return -1;
  }

  char ts[32];
  timestamp_utc(ts, sizeof(ts));
  fprintf(file,
          "{\"timestamp\":\"%s\",\"event\":\"%s\",\"request_id\":\"%s\",\"session_id\":\"%s\","
          "\"caller\":{\"uid\":%ld,\"gid\":%ld,\"pid\":%ld},",
          ts, event, request_id != NULL ? request_id : "", session_id != NULL ? session_id : "",
          (long)caller.uid, (long)caller.gid, (long)caller.pid);
  if (approver != NULL) {
    fprintf(file, "\"approver\":{\"uid\":%ld,\"gid\":%ld,\"pid\":%ld},", (long)approver->uid, (long)approver->gid, (long)approver->pid);
  }
  fprintf(file,
          "\"method\":\"%s\",\"risk\":\"%s\",\"decision\":\"%s\",\"error_kind\":\"%s\","
          "\"duration_ms\":%ld,\"params_summary\":\"%s\"}\n",
          method != NULL ? method : "", risk != NULL ? risk : "", decision != NULL ? decision : "",
          error_kind != NULL ? error_kind : "", duration_ms, params_summary != NULL ? params_summary : "");

  int failed = ferror(file);
  fclose(file);
  return failed ? -1 : 0;
}
