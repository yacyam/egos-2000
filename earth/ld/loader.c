#include "loader.h"

int initialization(uint ino) {
  char buff[] = "SUCCESS";
  grass->sys_tty(buff, 8, IO_WRITE);
  while (1);
}