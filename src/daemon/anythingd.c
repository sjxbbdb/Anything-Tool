#include "anything/approval.h"
#include "anything/audit.h"
#include "anything/config.h"
#include "anything/json.h"
#include "anything/policy.h"
#include "anything/rpc.h"
#include "anything/sys_info.h"
#include "anything/transport.h"

#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo) {
  (void)signo;
  g_stop = 1;
}

typedef struct daemon_state {
  anything_config config;
  anything_approval_store approvals;
} daemon_state;

static void send_invalid(int fd, const char *id, const char *kind, const char *message) {
  char response[ANYTHING_RPC_MAX_RESPONSE];
  anything_rpc_error(id, -32600, kind, message, "", response, sizeof(response));
  anything_transport_write_response(fd, response);
}

static int checked_audit(const anything_config *config, const char *event, const char *request_id, const char *session_id, anything_identity caller, const anything_identity *approver, const char *method, const char *risk, const char *decision, const char *error_kind, const char *params_summary, long duration_ms, int fd, const char *rpc_id) {
  if (anything_audit_write_event(config, event, request_id, session_id, caller, approver, method, risk, decision, error_kind, params_summary, duration_ms) == 0) {
    return 0;
  }
  char response[ANYTHING_RPC_MAX_RESPONSE];
  anything_rpc_error(rpc_id, -32050, "audit_failed", "audit write failed", "", response, sizeof(response));
  anything_transport_write_response(fd, response);
  return -1;
}

static void handle_tool_request(daemon_state *state, int fd, anything_identity caller, const char *body, size_t body_len) {
  char error[ANYTHING_MAX_ERROR_LEN];
  char response[ANYTHING_RPC_MAX_RESPONSE];
  anything_rpc_request request;
  if (anything_rpc_parse(body, body_len, &request, error, sizeof(error)) != 0) {
    send_invalid(fd, "null", "invalid_request", error);
    return;
  }

  if (checked_audit(&state->config, "request_received", "", request.session_id, caller, NULL, request.method, "", "received", "", "bounded request", 0, fd, request.id) != 0) {
    return;
  }
  if (strncmp(request.method, "approval.", 9) == 0) {
    if (checked_audit(&state->config, "preflight_denied", "", request.session_id, caller, NULL, request.method, "denied", "deny", "control_plane_denied", "agent tool socket cannot access approval methods", 0, fd, request.id) != 0) {
      return;
    }
    anything_rpc_error(request.id, -32040, "control_plane_denied", "approval methods require admin socket", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  anything_policy_result policy;
  anything_policy_preflight(&state->config, &request, caller, &policy);
  if (policy.decision == ANYTHING_POLICY_DENY) {
    if (checked_audit(&state->config, "preflight_denied", "", request.session_id, caller, NULL, request.method, policy.risk, "deny", policy.reason, policy.summary, 0, fd, request.id) != 0) {
      return;
    }
    anything_rpc_error(request.id, -32020, policy.reason, "policy denied", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  if (strcmp(request.method, "sys.info") != 0) {
    anything_rpc_error(request.id, -32601, "unknown_tool", "unknown method", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  anything_pending_request *pending = NULL;
  if (anything_approval_add(&state->approvals, &request, &policy, caller, state->config.approval_ttl_seconds, &pending) != 0) {
    anything_rpc_error(request.id, -32000, "resource_limit", "approval store full", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  char escaped_request_id[128];
  char escaped_risk[64];
  char escaped_summary[512];
  if (anything_json_escape_string(pending->request_id, escaped_request_id, sizeof(escaped_request_id)) != 0 ||
      anything_json_escape_string(pending->risk, escaped_risk, sizeof(escaped_risk)) != 0 ||
      anything_json_escape_string(pending->summary, escaped_summary, sizeof(escaped_summary)) != 0) {
    anything_rpc_error(request.id, -32000, "resource_limit", "approval response too large", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }
  char data[768];
  snprintf(data, sizeof(data), ",\"request_id\":\"%s\",\"risk\":\"%s\",\"summary\":\"%s\",\"expires_at\":%ld",
           escaped_request_id, escaped_risk, escaped_summary, (long)pending->expires_at);
  if (checked_audit(&state->config, "approval_required", pending->request_id, pending->session_id, caller, NULL, pending->method, pending->risk, "approval_required", "", pending->summary, 0, fd, request.id) != 0) {
    return;
  }
  anything_rpc_error(request.id, -32010, "approval_required", "approval required", data, response, sizeof(response));
  anything_transport_write_response(fd, response);
}

static int pending_to_request(const anything_pending_request *pending, anything_rpc_request *request) {
  memset(request, 0, sizeof(*request));
  snprintf(request->id, sizeof(request->id), "%s", pending->request_id);
  snprintf(request->method, sizeof(request->method), "%s", pending->method);
  snprintf(request->params, sizeof(request->params), "%s", pending->params);
  snprintf(request->session_id, sizeof(request->session_id), "%s", pending->session_id);
  request->request_hash = anything_rpc_hash_request(request->method, request->params, request->session_id);
  return 0;
}

static void handle_admin_execute(daemon_state *state, int fd, anything_identity admin, const anything_rpc_request *request) {
  char request_id[ANYTHING_APPROVAL_MAX_REQUEST_ID];
  char response[ANYTHING_RPC_MAX_RESPONSE];
  char error_kind[64];
  if (anything_rpc_extract_param_string(request->params, "request_id", request_id, sizeof(request_id)) != 0) {
    anything_rpc_error(request->id, -32602, "invalid_params", "request_id is required", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  anything_pending_request *pending = anything_approval_find(&state->approvals, request_id);
  anything_rpc_request original;
  if (pending != NULL) {
    pending_to_request(pending, &original);
  }
  uint64_t current_hash = pending != NULL ? original.request_hash : 0;
  if (anything_approval_validate_for_execute(pending, current_hash, error_kind, sizeof(error_kind)) != 0) {
    anything_rpc_error(request->id, -32011, error_kind, "approval cannot execute", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  anything_policy_result recheck;
  anything_policy_preflight(&state->config, &original, pending->requester, &recheck); /* policy recheck */
  if (recheck.decision == ANYTHING_POLICY_DENY || recheck.request_hash != pending->request_hash) {
    anything_rpc_error(request->id, -32021, "policy_denied", "policy recheck failed", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  if (checked_audit(&state->config, "execution_started", pending->request_id, pending->session_id, pending->requester, &admin, pending->method, pending->risk, "started", "", pending->summary, 0, fd, request->id) != 0) {
    return;
  }
  char result[2048];
  char sys_error[128];
  if (anything_sys_info_json(result, sizeof(result), sys_error, sizeof(sys_error)) != 0) {
    if (checked_audit(&state->config, "execution_failed", pending->request_id, pending->session_id, pending->requester, &admin, pending->method, pending->risk, "failed", "execution_failed", sys_error, 0, fd, request->id) != 0) {
      return;
    }
    anything_rpc_error(request->id, -32030, "execution_failed", sys_error, "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  pending->status = ANYTHING_APPROVAL_EXECUTED;
  if (checked_audit(&state->config, "execution_finished", pending->request_id, pending->session_id, pending->requester, &admin, pending->method, pending->risk, "finished", "", pending->summary, 0, fd, request->id) != 0) {
    return;
  }
  anything_rpc_result(request->id, result, response, sizeof(response));
  anything_transport_write_response(fd, response);
}

static void handle_admin_request(daemon_state *state, int fd, anything_identity admin, const char *body, size_t body_len) {
  char error[ANYTHING_MAX_ERROR_LEN];
  char response[ANYTHING_RPC_MAX_RESPONSE];
  anything_rpc_request request;
  if (anything_rpc_parse(body, body_len, &request, error, sizeof(error)) != 0) {
    send_invalid(fd, "null", "invalid_request", error);
    return;
  }

  if (!anything_config_identity_is_admin(&state->config, admin)) {
    if (checked_audit(&state->config, "preflight_denied", "", request.session_id, admin, NULL, request.method, "denied", "deny", "control_plane_denied", "admin caller is not in allowlist", 0, fd, request.id) != 0) {
      return;
    }
    anything_rpc_error(request.id, -32040, "control_plane_denied", "admin caller is not authorized", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }

  if (strcmp(request.method, "approval.list") == 0) {
    char result[4096];
    if (anything_approval_list_json(&state->approvals, result, sizeof(result)) != 0) {
      anything_rpc_error(request.id, -32000, "resource_limit", "pending list too large", "", response, sizeof(response));
    } else {
      anything_rpc_result(request.id, result, response, sizeof(response));
    }
    anything_transport_write_response(fd, response);
    return;
  }

  if (strcmp(request.method, "approval.execute") == 0) {
    handle_admin_execute(state, fd, admin, &request);
    return;
  }

  char request_id[ANYTHING_APPROVAL_MAX_REQUEST_ID];
  if (anything_rpc_extract_param_string(request.params, "request_id", request_id, sizeof(request_id)) != 0) {
    anything_rpc_error(request.id, -32602, "invalid_params", "request_id is required", "", response, sizeof(response));
    anything_transport_write_response(fd, response);
    return;
  }
  anything_pending_request *pending = anything_approval_find(&state->approvals, request_id);
  char error_kind[64];
  if (strcmp(request.method, "approval.approve") == 0) {
    if (anything_approval_grant(pending, admin, error_kind, sizeof(error_kind)) != 0) {
      anything_rpc_error(request.id, -32012, error_kind, "approval denied", "", response, sizeof(response));
    } else {
      if (checked_audit(&state->config, "approval_granted", pending->request_id, pending->session_id, pending->requester, &admin, pending->method, pending->risk, "granted", "", pending->summary, 0, fd, request.id) != 0) {
        return;
      }
      anything_rpc_result(request.id, "{\"approved\":true}", response, sizeof(response));
    }
  } else if (strcmp(request.method, "approval.reject") == 0) {
    if (anything_approval_reject(pending, admin, error_kind, sizeof(error_kind)) != 0) {
      anything_rpc_error(request.id, -32013, error_kind, "approval rejection failed", "", response, sizeof(response));
    } else {
      if (checked_audit(&state->config, "approval_rejected", pending->request_id, pending->session_id, pending->requester, &admin, pending->method, pending->risk, "rejected", "", pending->summary, 0, fd, request.id) != 0) {
        return;
      }
      anything_rpc_result(request.id, "{\"rejected\":true}", response, sizeof(response));
    }
  } else {
    anything_rpc_error(request.id, -32601, "unknown_tool", "unknown admin method", "", response, sizeof(response));
  }
  anything_transport_write_response(fd, response);
}

static void accept_one(daemon_state *state, int listen_fd, anything_socket_plane plane) {
  char error[ANYTHING_MAX_ERROR_LEN];
  int fd = accept(listen_fd, NULL, NULL);
  if (fd < 0) {
    return;
  }

  anything_identity peer;
  if (anything_transport_peer_identity(fd, &peer, error, sizeof(error)) != 0) {
    send_invalid(fd, "null", "control_plane_denied", error);
    close(fd);
    return;
  }
  if (anything_transport_set_read_timeout(fd, state->config.read_timeout_ms, error, sizeof(error)) != 0) {
    send_invalid(fd, "null", "resource_limit", error);
    close(fd);
    return;
  }

  char *buffer = calloc(1, state->config.max_request_bytes + 1);
  if (buffer == NULL) {
    send_invalid(fd, "null", "resource_limit", "allocation failed");
    close(fd);
    return;
  }
  size_t body_len = 0;
  if (anything_transport_read_request(fd, buffer, state->config.max_request_bytes, &body_len, error, sizeof(error)) != 0) {
    send_invalid(fd, "null", "resource_limit", error);
    free(buffer);
    close(fd);
    return;
  }

  if (plane == ANYTHING_SOCKET_TOOL) {
    handle_tool_request(state, fd, peer, buffer, body_len);
  } else {
    handle_admin_request(state, fd, peer, buffer, body_len);
  }
  free(buffer);
  close(fd);
}

int main(int argc, char **argv) {
  const char *config_path = argc > 1 ? argv[1] : "config/anythingd.example.toml";
  daemon_state state;
  char error[ANYTHING_MAX_ERROR_LEN];
  if (anything_config_load(config_path, &state.config, error, sizeof(error)) != 0) {
    fprintf(stderr, "anythingd config error: %s\n", error);
    return 2;
  }
  if (anything_audit_validate_startup(&state.config, error, sizeof(error)) != 0) {
    fprintf(stderr, "anythingd audit error: %s\n", error);
    return 2;
  }
  anything_approval_store_init(&state.approvals);

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  int tool_fd = anything_transport_listen(state.config.tool_socket_path, 0600, error, sizeof(error));
  if (tool_fd < 0) {
    fprintf(stderr, "anythingd tool socket error: %s\n", error);
    return 2;
  }
  int admin_fd = anything_transport_listen(state.config.admin_socket_path, 0600, error, sizeof(error));
  if (admin_fd < 0) {
    fprintf(stderr, "anythingd admin socket error: %s\n", error);
    anything_transport_cleanup_socket(state.config.tool_socket_path);
    return 2;
  }

  fprintf(stderr, "anythingd %s listening tool=%s admin=%s\n", ANYTHING_VERSION, state.config.tool_socket_path, state.config.admin_socket_path);
  while (!g_stop) {
    struct pollfd fds[2] = {
        {.fd = tool_fd, .events = POLLIN},
        {.fd = admin_fd, .events = POLLIN},
    };
    int ready = poll(fds, 2, 500);
    if (ready <= 0) {
      continue;
    }
    if (fds[0].revents & POLLIN) {
      accept_one(&state, tool_fd, ANYTHING_SOCKET_TOOL);
    }
    if (fds[1].revents & POLLIN) {
      accept_one(&state, admin_fd, ANYTHING_SOCKET_ADMIN);
    }
  }

  close(tool_fd);
  close(admin_fd);
  anything_transport_cleanup_socket(state.config.tool_socket_path);
  anything_transport_cleanup_socket(state.config.admin_socket_path);
  return 0;
}
