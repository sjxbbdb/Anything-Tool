#include "anything/rpc.h"

#include "anything/json.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct rpc_scan {
  int seen_jsonrpc;
  int seen_id;
  int seen_method;
  int seen_params;
} rpc_scan;

static const char *skip_ws(const char *p) {
  while (*p != '\0' && isspace((unsigned char)*p)) {
    p++;
  }
  return p;
}

static int append_decoded(char *out, size_t out_len, size_t *used, char c) {
  if (*used + 1 >= out_len) {
    return -1;
  }
  out[(*used)++] = c;
  out[*used] = '\0';
  return 0;
}

static int parse_json_string(const char **cursor, char *out, size_t out_len) {
  const char *p = *cursor;
  if (*p != '"' || out_len == 0) {
    return -1;
  }
  p++;
  size_t used = 0;
  out[0] = '\0';
  while (*p != '\0') {
    if (*p == '"') {
      *cursor = p + 1;
      return 0;
    }
    if ((unsigned char)*p < 0x20) {
      return -1;
    }
    if (*p == '\\') {
      p++;
      switch (*p) {
        case '"':
        case '\\':
          if (append_decoded(out, out_len, &used, *p) != 0) return -1;
          break;
        case 'n':
          if (append_decoded(out, out_len, &used, '\n') != 0) return -1;
          break;
        case 'r':
          if (append_decoded(out, out_len, &used, '\r') != 0) return -1;
          break;
        case 't':
          if (append_decoded(out, out_len, &used, '\t') != 0) return -1;
          break;
        default:
          return -1;
      }
      p++;
      continue;
    }
    if (append_decoded(out, out_len, &used, *p) != 0) {
      return -1;
    }
    p++;
  }
  return -1;
}

static int skip_json_string(const char **cursor) {
  const char *p = *cursor;
  if (*p != '"') {
    return -1;
  }
  p++;
  while (*p != '\0') {
    if (*p == '"') {
      *cursor = p + 1;
      return 0;
    }
    if ((unsigned char)*p < 0x20) {
      return -1;
    }
    if (*p == '\\') {
      p++;
      switch (*p) {
        case '"':
        case '\\':
        case 'n':
        case 'r':
        case 't':
          p++;
          continue;
        default:
          return -1;
      }
    }
    p++;
  }
  return -1;
}

static int copy_json_object_raw(const char **cursor, char *out, size_t out_len) {
  const char *start = *cursor;
  if (*start != '{') {
    return -1; /* non-object params */
  }
  const char *p = start;
  int depth = 0;
  while (*p != '\0') {
    if (*p == '"') {
      if (skip_json_string(&p) != 0) {
        return -1;
      }
      continue;
    }
    if (*p == '{') {
      depth++;
    } else if (*p == '}') {
      depth--;
      if (depth == 0) {
        p++;
        size_t len = (size_t)(p - start);
        if (len >= out_len) {
          return -1;
        }
        memcpy(out, start, len);
        out[len] = '\0';
        *cursor = p;
        return 0;
      }
    }
    p++;
  }
  return -1;
}

static int skip_json_value(const char **cursor) {
  const char *p = skip_ws(*cursor);
  if (*p == '"') {
    if (skip_json_string(&p) != 0) {
      return -1;
    }
    *cursor = p;
    return 0;
  }
  if (*p == '{') {
    char scratch[ANYTHING_RPC_MAX_PARAMS];
    if (copy_json_object_raw(&p, scratch, sizeof(scratch)) != 0) {
      return -1;
    }
    *cursor = p;
    return 0;
  }
  if (*p == '[') {
    int depth = 0;
    while (*p != '\0') {
      if (*p == '"') {
        if (skip_json_string(&p) != 0) return -1;
        continue;
      }
      if (*p == '[') depth++;
      if (*p == ']') {
        depth--;
        if (depth == 0) {
          *cursor = p + 1;
          return 0;
        }
      }
      p++;
    }
    return -1;
  }
  while (*p != '\0' && *p != ',' && *p != '}' && *p != ']') {
    p++;
  }
  *cursor = p;
  return 0;
}

static int mark_seen(int *seen, char *error, size_t error_len) {
  if (*seen) {
    snprintf(error, error_len, "duplicate top-level key");
    return -1;
  }
  *seen = 1;
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
  if (json == NULL || len == 0) {
    snprintf(error, error_len, "invalid JSON");
    return -1;
  }
  memset(request, 0, sizeof(*request));
  rpc_scan scan = {0};

  const char *p = skip_ws(json);
  if (*p != '{') {
    snprintf(error, error_len, "invalid JSON object");
    return -1;
  }
  p++;
  p = skip_ws(p);
  if (*p == '}') {
    snprintf(error, error_len, "invalid JSON-RPC request");
    return -1;
  }

  while (*p != '\0') {
    char key[64];
    if (parse_json_string(&p, key, sizeof(key)) != 0) {
      snprintf(error, error_len, "invalid top-level key");
      return -1;
    }
    p = skip_ws(p);
    if (*p != ':') {
      snprintf(error, error_len, "invalid request shape");
      return -1;
    }
    p++;
    p = skip_ws(p);

    if (strcmp(key, "jsonrpc") == 0) {
      if (mark_seen(&scan.seen_jsonrpc, error, error_len) != 0) return -1;
      char version[16];
      if (parse_json_string(&p, version, sizeof(version)) != 0 || strcmp(version, "2.0") != 0) {
        snprintf(error, error_len, "invalid JSON-RPC version");
        return -1;
      }
    } else if (strcmp(key, "id") == 0) {
      if (mark_seen(&scan.seen_id, error, error_len) != 0) return -1;
      if (parse_json_string(&p, request->id, sizeof(request->id)) != 0) {
        snprintf(error, error_len, "invalid id");
        return -1;
      }
    } else if (strcmp(key, "method") == 0) {
      if (mark_seen(&scan.seen_method, error, error_len) != 0) return -1;
      if (parse_json_string(&p, request->method, sizeof(request->method)) != 0) {
        snprintf(error, error_len, "invalid method");
        return -1;
      }
    } else if (strcmp(key, "params") == 0) {
      if (mark_seen(&scan.seen_params, error, error_len) != 0) return -1;
      if (*p != '{') {
        snprintf(error, error_len, "non-object params");
        return -1;
      }
      if (copy_json_object_raw(&p, request->params, sizeof(request->params)) != 0) {
        snprintf(error, error_len, "invalid params");
        return -1;
      }
    } else if (skip_json_value(&p) != 0) {
      snprintf(error, error_len, "invalid value");
      return -1;
    }

    p = skip_ws(p);
    if (*p == ',') {
      p++;
      p = skip_ws(p);
      continue;
    }
    if (*p == '}') {
      p++;
      break;
    }
    snprintf(error, error_len, "invalid request terminator");
    return -1;
  }

  p = skip_ws(p);
  if (*p != '\0' && *p != '\n') {
    snprintf(error, error_len, "trailing data");
    return -1;
  }
  if (!scan.seen_jsonrpc || !scan.seen_id || !scan.seen_method) {
    snprintf(error, error_len, "missing required JSON-RPC field");
    return -1;
  }
  if (!scan.seen_params) {
    snprintf(request->params, sizeof(request->params), "{}");
  }
  /* nested top-level key confusion is prevented by scanning only depth-1 fields. */
  if (anything_rpc_extract_param_string(request->params, "session_id", request->session_id, sizeof(request->session_id)) != 0) {
    snprintf(request->session_id, sizeof(request->session_id), "default");
  }
  request->request_hash = anything_rpc_hash_request(request->method, request->params, request->session_id);
  return 0;
}

int anything_rpc_result(const char *id, const char *json_result, char *out, size_t out_len) {
  char escaped_id[ANYTHING_RPC_MAX_ID * 2];
  if (anything_json_escape_string(id, escaped_id, sizeof(escaped_id)) != 0) {
    return -1;
  }
  return snprintf(out, out_len, "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"result\":%s}\n", escaped_id, json_result);
}

int anything_rpc_error(const char *id, int code, const char *kind, const char *message, const char *data_json, char *out, size_t out_len) {
  char escaped_id[ANYTHING_RPC_MAX_ID * 2];
  char escaped_kind[128];
  char escaped_message[256];
  if (anything_json_escape_string(id != NULL ? id : "null", escaped_id, sizeof(escaped_id)) != 0 ||
      anything_json_escape_string(kind, escaped_kind, sizeof(escaped_kind)) != 0 ||
      anything_json_escape_string(message, escaped_message, sizeof(escaped_message)) != 0) {
    return -1;
  }
  const char *extra = data_json != NULL && data_json[0] != '\0' ? data_json : "";
  return snprintf(out, out_len,
                  "{\"jsonrpc\":\"2.0\",\"id\":\"%s\",\"error\":{\"code\":%d,\"message\":\"%s\",\"data\":{\"kind\":\"%s\"%s}}}\n",
                  escaped_id, code, escaped_message, escaped_kind, extra);
}

int anything_rpc_extract_param_string(const char *params, const char *key, char *out, size_t out_len) {
  const char *p = skip_ws(params);
  if (*p != '{') {
    return -1;
  }
  p++;
  p = skip_ws(p);
  while (*p != '\0' && *p != '}') {
    char found_key[64];
    if (parse_json_string(&p, found_key, sizeof(found_key)) != 0) {
      return -1;
    }
    p = skip_ws(p);
    if (*p != ':') {
      return -1;
    }
    p++;
    p = skip_ws(p);
    if (strcmp(found_key, key) == 0) {
      return parse_json_string(&p, out, out_len);
    }
    if (skip_json_value(&p) != 0) {
      return -1;
    }
    p = skip_ws(p);
    if (*p == ',') {
      p++;
      p = skip_ws(p);
    }
  }
  return -1;
}
