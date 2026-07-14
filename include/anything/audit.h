#ifndef ANYTHING_AUDIT_H
#define ANYTHING_AUDIT_H

#include "anything/approval.h"
#include "anything/config.h"
#include "anything/identity.h"

#define ANYTHING_AUDIT_MAX_ERROR 128

int anything_audit_write_event(const anything_config *config, const char *event, const char *request_id, const char *session_id, anything_identity caller, const anything_identity *approver, const char *method, const char *risk, const char *decision, const char *error_kind, const char *params_summary, long duration_ms);

#endif
