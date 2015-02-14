#include <errno.h>
#include <stdbool.h>

#ifdef DARWIN
int setUid();
#endif

int togglePac(bool turnOn, const char* pacUrl);

enum RET_ERRORS {
  RET_NO_ERROR = 0,
  INVALID_FORMAT = 1,
  NO_PERMISSION = 2,
  SYSCALL_FAILED = 3,
  NO_MEMORY = 4
};
