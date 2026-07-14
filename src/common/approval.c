#include "anything/approval.h"

#include "anything/json.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static void copy_error(char *error_kind, size_t error_kind_len, const char *kind) {
  if (error_kind != NULL && error_kind_len > 0) {
    snprintf(error_kind, error_kind_len, "%s", kind);
  }
}

void anything_approval_store_init(anything_approval_store *store) {
  memset(store, 0, sizeof(*store));
  store->next_id = 1;
}

int anything_approval_add(anything_approval_store *store, const anything_rpc_request *request, const anything_policy_result *policy, anything_identity requester, int ttl_seconds, anything_pending_request **created) {
  if (store->count >= ANYTHING_APPROVAL_MAX_PENDING) {
    return -1;
  }
  anything_pending_request *pending = &store->pending[store->count++];
  memset(pending, 0, sizeof(*pending));
  snprintf(pending->request_id, sizeof(pending->request_id), "req_%lu", store->next_id++);
  pending->request_hash = policy->request_hash;
  pending->requester = requester;
  snprintf(pending->session_id, sizeof(pending->session_id), "%s", request->session_id);
  snprintf(pending->method, sizeof(pending->method), "%s", request->method);
  snprintf(pending->params, sizeof(pending->params), "%s", request->params);
  snprintf(pending->risk, sizeof(pending->risk), "%s", policy->risk);
  snprintf(pending->summary, sizeof(pending->summary), "%s", policy->summary);
  pending->expires_at = time(NULL) + ttl_seconds;
  pending->status = ANYTHING_APPROVAL_PENDING;
  *created = pending;
  return 0;
}

anything_pending_request *anything_approval_find(anything_approval_store *store, const char *request_id) {
  for (size_t i = 0; i < store->count; i++) {
    if (strcmp(store->pending[i].request_id, request_id) == 0) {
      return &store->pending[i];
    }
  }
  return NULL;
}

int anything_approval_grant(anything_pending_request *pending, anything_identity approver, char *error_kind, size_t error_kind_len) {
  if (pending == NULL) {
    copy_error(error_kind, error_kind_len, "approval_not_found");
    return -1;
  }
  if (pending->status != ANYTHING_APPROVAL_PENDING) {
    copy_error(error_kind, error_kind_len, "approval_not_pending");
    return -1;
  }
  if (time(NULL) > pending->expires_at) {
    copy_error(error_kind, error_kind_len, "approval_expired");
    return -1;
  }
  if (anything_identity_same(pending->requester, approver)) {
    copy_error(error_kind, error_kind_len, "control_plane_denied"); /* requester cannot approve */
    return -1;
  }
  pending->approver = approver;
  pending->status = ANYTHING_APPROVAL_GRANTED;
  return 0;
}

int anything_approval_reject(anything_pending_request *pending, anything_identity approver, char *error_kind, size_t error_kind_len) {
  if (pending == NULL) {
    copy_error(error_kind, error_kind_len, "approval_not_found");
    return -1;
  }
  if (anything_identity_same(pending->requester, approver)) {
    copy_error(error_kind, error_kind_len, "control_plane_denied");
    return -1;
  }
  pending->approver = approver;
  pending->status = ANYTHING_APPROVAL_REJECTED;
  return 0;
}

int anything_approval_validate_for_execute(const anything_pending_request *pending, uint64_t current_hash, char *error_kind, size_t error_kind_len) {
  if (pending == NULL) {
    copy_error(error_kind, error_kind_len, "approval_not_found");
    return -1;
  }
  if (pending->status != ANYTHING_APPROVAL_GRANTED) {
    copy_error(error_kind, error_kind_len, pending->status == ANYTHING_APPROVAL_REJECTED ? "approval_rejected" : "approval_required");
    return -1;
  }
  if (time(NULL) > pending->expires_at) {
    copy_error(error_kind, error_kind_len, "approval_expired");
    return -1;
  }
  if (pending->request_hash != current_hash) {
    copy_error(error_kind, error_kind_len, "request_changed");
    return -1;
  }
  return 0;
}

int anything_approval_list_json(const anything_approval_store *store, char *out, size_t out_len) {
  size_t used = 0;
  int wrote = snprintf(out + used, out_len - used, "{\"pending\":[");
  if (wrote < 0 || (size_t)wrote >= out_len - used) {
    return -1;
  }
  used += (size_t)wrote;
  for (size_t i = 0; i < store->count; i++) {
    const anything_pending_request *p = &store->pending[i];
    char request_id[128];
    char method[128];
    char risk[64];
    char summary[512];
    if (anything_json_escape_string(p->request_id, request_id, sizeof(request_id)) != 0 ||
        anything_json_escape_string(p->method, method, sizeof(method)) != 0 ||
        anything_json_escape_string(p->risk, risk, sizeof(risk)) != 0 ||
        anything_json_escape_string(p->summary, summary, sizeof(summary)) != 0) {
      return -1;
    }
    wrote = snprintf(out + used, out_len - used,
                     "%s{\"request_id\":\"%s\",\"method\":\"%s\",\"risk\":\"%s\",\"status\":%d,\"summary\":\"%s\"}",
                     i == 0 ? "" : ",", request_id, method, risk, (int)p->status, summary);
    if (wrote < 0 || (size_t)wrote >= out_len - used) {
      return -1;
    }
    used += (size_t)wrote;
  }
  wrote = snprintf(out + used, out_len - used, "]}");
  return wrote < 0 || (size_t)wrote >= out_len - used ? -1 : 0;
}
