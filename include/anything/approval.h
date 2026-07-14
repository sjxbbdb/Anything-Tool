#ifndef ANYTHING_APPROVAL_H
#define ANYTHING_APPROVAL_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "anything/identity.h"
#include "anything/policy.h"

#define ANYTHING_APPROVAL_MAX_PENDING 64
#define ANYTHING_APPROVAL_MAX_REQUEST_ID 40
#define ANYTHING_APPROVAL_MAX_METHOD 64
#define ANYTHING_APPROVAL_MAX_PARAMS 1024
#define ANYTHING_APPROVAL_MAX_SESSION 64
#define ANYTHING_APPROVAL_MAX_SUMMARY 256

typedef enum anything_approval_status {
  ANYTHING_APPROVAL_PENDING = 0,
  ANYTHING_APPROVAL_GRANTED = 1,
  ANYTHING_APPROVAL_REJECTED = 2,
  ANYTHING_APPROVAL_EXECUTED = 3
} anything_approval_status;

typedef struct anything_pending_request {
  char request_id[ANYTHING_APPROVAL_MAX_REQUEST_ID];
  uint64_t request_hash;
  anything_identity requester;
  anything_identity approver;
  char session_id[ANYTHING_APPROVAL_MAX_SESSION];
  char method[ANYTHING_APPROVAL_MAX_METHOD];
  char params[ANYTHING_APPROVAL_MAX_PARAMS];
  char risk[16];
  char summary[ANYTHING_APPROVAL_MAX_SUMMARY];
  time_t expires_at;
  anything_approval_status status;
} anything_pending_request;

typedef struct anything_approval_store {
  anything_pending_request pending[ANYTHING_APPROVAL_MAX_PENDING];
  size_t count;
  unsigned long next_id;
} anything_approval_store;

void anything_approval_store_init(anything_approval_store *store);
int anything_approval_add(anything_approval_store *store, const anything_rpc_request *request, const anything_policy_result *policy, anything_identity requester, int ttl_seconds, anything_pending_request **created);
anything_pending_request *anything_approval_find(anything_approval_store *store, const char *request_id);
int anything_approval_grant(anything_pending_request *pending, anything_identity approver, char *error_kind, size_t error_kind_len);
int anything_approval_reject(anything_pending_request *pending, anything_identity approver, char *error_kind, size_t error_kind_len);
int anything_approval_validate_for_execute(const anything_pending_request *pending, uint64_t current_hash, char *error_kind, size_t error_kind_len);
int anything_approval_list_json(const anything_approval_store *store, char *out, size_t out_len);

#endif
