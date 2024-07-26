#include "egos.h"
#define NSEGMENTS 5

struct segment {
  void *base_vaddr;
  uint rwx, memsz, filesz, fileoff;
};

struct segment_table {
  struct segment segmentbl[NSEGMENTS];
};

struct grass *grass = (struct grass *)GRASS_STRUCT_BASE;
struct earth *earth = (struct earth *)EARTH_STRUCT_BASE;