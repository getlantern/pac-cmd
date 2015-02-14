#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

void usage()
{
  puts("Usage: pac [on  <pac url> | off]");
  exit(INVALID_FORMAT);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    usage();
  }

#ifdef DARWIN
  if (strcmp(argv[1], "setuid") == 0) {
    return setUid();
  }
#endif

  if (strcmp(argv[1], "on") == 0) {
    if (argc < 3) {
      usage();
    }
    return togglePac(true, argv[2]);
  } else if (strcmp(argv[1], "off") == 0) {
    return togglePac(false, "");
  } else {
    usage();
  }
  // code never reaches here, just stops compiler from complain
  return RET_NO_ERROR;
}
