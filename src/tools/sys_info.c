#include "anything/sys_info.h"

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

static void strip_newline(char *text) {
  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
    text[--len] = '\0';
  }
}

static void read_first_line(const char *path, char *out, size_t out_len) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    snprintf(out, out_len, "unknown");
    return;
  }
  if (fgets(out, (int)out_len, file) == NULL) {
    snprintf(out, out_len, "unknown");
  }
  fclose(file);
  strip_newline(out);
}

int anything_sys_info_json(char *out, size_t out_len, char *error, size_t error_len) {
  struct utsname uts;
  if (uname(&uts) != 0) {
    snprintf(error, error_len, "uname failed");
    return -1;
  }

  char uptime[64];
  char os_release[256];
  read_first_line("/proc/uptime", uptime, sizeof(uptime));
  read_first_line("/etc/os-release", os_release, sizeof(os_release));

  int wrote = snprintf(out, out_len,
                       "{\"version\":\"v0.1.0\",\"hostname\":\"%s\",\"kernel_version\":\"%s\","
                       "\"os_release\":\"%s\",\"uptime\":\"%s\",\"architecture\":\"%s\"}",
                       uts.nodename, uts.release, os_release, uptime, uts.machine);
  if (wrote < 0 || (size_t)wrote >= out_len) {
    snprintf(error, error_len, "sys.info output exceeded buffer");
    return -1;
  }
  return 0;
}
