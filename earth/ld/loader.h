#include "egos.h"
#define NSEGMENTS 5

struct segment {
  void *base_vaddr;
  uint rwx, memsz, filesz, fileoff;
};

struct segment_table {
  struct segment segmentbl[NSEGMENTS];
};

struct grass *grass = (struct grass *)APPS_STACK_TOP;
struct earth *earth = (struct earth *)GRASS_STACK_TOP;