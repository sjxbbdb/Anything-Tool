#include "anything/rpc.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int copy_json_string_value(const char *json, const char *key, char *out, size_t out_len) {
  char needle[80];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char *found = strstr(json, needle);
  if (found == NULL) {
    return -1;
  }
  const char *colon = strchr(found + strlen(needle), ':');
  if (colon == NULL) {
    return -1;
  }
  const char *quote = strchr(colon, '"');
  if (quote == NULL) {
    return -1;
  }
  quote++;
  size_t used = 0;
  while (quote[used] != '\0' && quote[used] != '"') {
    if (quote[used] == '\\') {
      return -1;
    }
    used++;
  }
  if (quote[used] != '"' || used >= out_len) {
    return -1;
  }
  memcpy(out, quote, used);
  out[used] = '\0';
  return 0;
}

static int copy_json_object_value(const char *json, const char *key, char *out, size_t out_len) {
  char needle[80];
  snprintf(needle, sizeof(needle), "\"%s\"", key);
  const char *found = strstr(json, needle);
  if (found == NULL) {
    out[0] = '\0';
    return 0;
  }
  const char *colon = strchr(found + strlen(needle), ':');
  if (colon == NULL) {
    return -1;
  }
  const char *start = colon + 1;
  while (isspace((unsigned char)*start)) {
    start++;
  }
  if (*start != '{') {
    return -1;
  }
  int depth = 0;
  int in_string = 0;
  size_t i = 0;
  for (; start[i] != '\0'; i++) {
    char c = start[i];
    if (c == '"' && (i == 0 || start[i - 1] != '\\')) {
      in_string = !in_string;
    }
    if (!in_string) {
      if (c == '{') {
        depth++;
      } else if (c == '}') {
        depth--;
        if (depth == 0) {
          i++;
          break;
        }
      }
    }
  }
  if (depth != 0 || i >= out_len) {
    return -1;
  }
  memcpy(out, start, i);
  out[i] = '\0';
  return 0;
}

uint64_t anything_rpc_hash_request(const char *method, const char *params, const char *session_id) {
  uint64_t hash = 1469598103934665603ULL;
  const char *parts[] = {method, "|", params, "|", session_id};
  for (size_t p = 0; p < sizeof(parts) / sizeof(parts[0]); p++) {
    for (const unsigned char *s = (const unsigned char *)parts[p]; *s != '\0'; s++) {
      hash ^= (uint64_t)*s;
      hash *= 1099511628211ULL;
    }
  }
  return hash;
}

int anything_rpc_parse(const char *json, size_t len, anything_rpc_request *request, char *error, size_t error_len) {
  if (json == NULL || len == 0 || json[0] != '{') {
    snprintf(error, error_len, "invalid JSON");
    return -1;
  }
  memset(request, 0, sizeof(*request));
  if (copy_json_string_value(json, "jsonrpc", request->id, sizeof(request->id)) != 0 || strcmp(request->id, "2.0") != 0) {
    snprintf(error, error_len, "invalid JSON-RPC version");
    return -1;
  }
  if (copy_json_string_value(json, "id", request->id, sizeof(request->id)) != 0) {
    snprintf(error, error_len, "missing id");
    return -1;
  }
  if (copy_json_string_value(json, "method", request->method, sizeof(request->method)) != 0) {
    snprintf(error, error_len, "missing method");
    return -1;
  }
  if (copy_json_object_value(json, "params", request->params, sizeof(request->params)) != 0) {
    snprintf(error, error_len, "invalid params");
    return -1;
  }
  if (request->params[0] == '\0') {
    snprintf(request->params, sizeof(request->params), "{}");
  }
  if (anything_rpc_extract_param_string(request->params, "session_id", request->session_id, sizeof(request->session_id)) != 0) {
    snprintf(request->session_id, sizeof(request->session_id), "default");
  }
  request->request_hash = anything_rpc_hash_request(request->method, request->params, request->session_id);
  return 0;
}

int anything_rpc_result(const char *id, const char *json_result, char *out, size_t out_len) {
  return snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}\n", id, json_result);
}

int anything_rpc_error(const char *id, int code, const char *kind, const char *message, const char *data_json, char *out, size_t out_len) {
  const char *extra = data_json != NULL && data_json[0] != '\0' ? data_json : "";
  return snprintf(out, out_len,
                  "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"error\":{\"code\":%d,\"message\":\"%s\",\"data\":{\"kind\":\"%s\"%s}}}\n",
                  id != NULL ? id : "null", code, message, kind, extra);
}

int anything_rpc_extract_param_string(const char *params, const char *key, char *out, size_t out_len) {
  return copy_json_string_value(params, key, out, out_len);
}
