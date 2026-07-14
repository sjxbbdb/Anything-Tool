#include "anything/policy.h"

#include <stdio.h>
#include <string.h>

int anything_policy_preflight(const anything_config *config, const anything_rpc_request *request, anything_identity caller, anything_policy_result *result) {
  (void)caller;
  memset(result, 0, sizeof(*result));
  result->request_hash = request->request_hash; /* request_hash participates in approval and policy recheck */

  if (strcmp(request->method, "sys.info") != 0) {
    result->decision = ANYTHING_POLICY_DENY;
    snprintf(result->risk, sizeof(result->risk), "denied");
    snprintf(result->reason, sizeof(result->reason), "unknown_tool");
    snprintf(result->summary, sizeof(result->summary), "Unknown method %s", request->method);
    return 0;
  }

  if (!config->capability_sys_read) {
    result->decision = ANYTHING_POLICY_DENY;
    snprintf(result->risk, sizeof(result->risk), "denied");
    snprintf(result->reason, sizeof(result->reason), "policy_denied");
    snprintf(result->summary, sizeof(result->summary), "sys.info denied because capabilities.sys_read is disabled");
    return 0;
  }

  result->decision = ANYTHING_POLICY_APPROVAL_REQUIRED;
  snprintf(result->risk, sizeof(result->risk), "low");
  snprintf(result->reason, sizeof(result->reason), "approval_required");
  snprintf(result->summary, sizeof(result->summary), "Read Linux system information with sys.info");
  return 0;
}
