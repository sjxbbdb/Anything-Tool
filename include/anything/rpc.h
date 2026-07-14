#ifndef ANYTHING_RPC_H
#define ANYTHING_RPC_H

#include <stddef.h>
#include <stdint.h>

#define ANYTHING_RPC_MAX_ID 64
#define ANYTHING_RPC_MAX_METHOD 64
#define ANYTHING_RPC_MAX_SESSION 64
#define ANYTHING_RPC_MAX_PARAMS 1024
#define ANYTHING_RPC_MAX_RESPONSE 4096

typedef struct anything_rpc_request {
  char id[ANYTHING_RPC_MAX_ID];
  char method[ANYTHING_RPC_MAX_METHOD];
  char session_id[ANYTHING_RPC_MAX_SESSION];
  char params[ANYTHING_RPC_MAX_PARAMS];
  uint64_t request_hash;
} anything_rpc_request;

int anything_rpc_parse(const char *json, size_t len, anything_rpc_request *request, char *error, size_t error_len);
uint64_t anything_rpc_hash_request(const char *method, const char *params, const char *session_id);
int anything_rpc_result(const char *id, const char *json_result, char *out, size_t out_len);
int anything_rpc_error(const char *id, int code, const char *kind, const char *message, const char *data_json, char *out, size_t out_len);
int anything_rpc_extract_param_string(const char *params, const char *key, char *out, size_t out_len);

#endif
