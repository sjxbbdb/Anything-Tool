#ifndef ANYTHING_JSON_H
#define ANYTHING_JSON_H

#include <stddef.h>

int anything_json_escape_string(const char *input, char *out, size_t out_len);
int anything_json_write_string_field(char *out, size_t out_len, const char *name, const char *value);

#endif
