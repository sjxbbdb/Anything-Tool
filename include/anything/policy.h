#ifndef ANYTHING_POLICY_H
#define ANYTHING_POLICY_H

#include "anything/config.h"
#include "anything/identity.h"
#include "anything/rpc.h"

#define ANYTHING_POLICY_MAX_SUMMARY 256
#define ANYTHING_POLICY_MAX_REASON 128

typedef enum anything_policy_decision {
  ANYTHING_POLICY_DENY = 0,
  ANYTHING_POLICY_APPROVAL_REQUIRED = 1,
  ANYTHING_POLICY_ALLOW = 2
} anything_policy_decision;

typedef struct anything_policy_result {
  anything_policy_decision decision;
  char risk[16];
  uint64_t request_hash;
  char summary[ANYTHING_POLICY_MAX_SUMMARY];
  char reason[ANYTHING_POLICY_MAX_REASON];
} anything_policy_result;

int anything_policy_preflight(const anything_config *config, const anything_rpc_request *request, anything_identity caller, anything_policy_result *result);

#endif
