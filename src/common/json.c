#include "anything/json.h"

#include <stdio.h>

static int append_char(char *out, size_t out_len, size_t *used, char c) {
  if (*used + 1 >= out_len) {
    return -1;
  }
  out[(*used)++] = c;
  out[*used] = '\0';
  return 0;
}

static int append_text(char *out, size_t out_len, size_t *used, const char *text) {
  for (const char *p = text; *p != '\0'; p++) {
    if (append_char(out, out_len, used, *p) != 0) {
      return -1;
    }
  }
  return 0;
}

int anything_json_escape_string(const char *input, char *out, size_t out_len) {
  if (out == NULL || out_len == 0) {
    return -1;
  }
  out[0] = '\0';
  if (input == NULL) {
    return 0;
  }

  size_t used = 0;
  for (const unsigned char *p = (const unsigned char *)input; *p != '\0'; p++) {
    switch (*p) {
      case '"':
        if (append_text(out, out_len, &used, "\\\"") != 0) return -1;
        break;
      case '\\':
        if (append_text(out, out_len, &used, "\\\\") != 0) return -1;
        break;
      case '\n':
        if (append_text(out, out_len, &used, "\\n") != 0) return -1;
        break;
      case '\r':
        if (append_text(out, out_len, &used, "\\r") != 0) return -1;
        break;
      case '\t':
        if (append_text(out, out_len, &used, "\\t") != 0) return -1;
        break;
      default:
        if (*p < 0x20) {
          int wrote = snprintf(out + used, out_len - used, "\\u%04x", *p);
          if (wrote < 0 || (size_t)wrote >= out_len - used) {
            return -1;
          }
          used += (size_t)wrote;
        } else if (append_char(out, out_len, &used, (char)*p) != 0) {
          return -1;
        }
        break;
    }
  }
  return 0;
}

int anything_json_write_string_field(char *out, size_t out_len, const char *name, const char *value) {
  char escaped_name[128];
  char escaped_value[2048];
  if (anything_json_escape_string(name, escaped_name, sizeof(escaped_name)) != 0) {
    return -1;
  }
  if (anything_json_escape_string(value, escaped_value, sizeof(escaped_value)) != 0) {
    return -1;
  }
  int wrote = snprintf(out, out_len, "\"%s\":\"%s\"", escaped_name, escaped_value);
  return wrote < 0 || (size_t)wrote >= out_len ? -1 : 0;
}
