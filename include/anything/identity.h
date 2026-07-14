#ifndef ANYTHING_IDENTITY_H
#define ANYTHING_IDENTITY_H

#include <sys/types.h>

typedef struct anything_identity {
  uid_t uid;
  gid_t gid;
  pid_t pid;
} anything_identity;

static inline int anything_identity_same(anything_identity a, anything_identity b) {
  return a.uid == b.uid && a.gid == b.gid && a.pid == b.pid;
}

#endif
