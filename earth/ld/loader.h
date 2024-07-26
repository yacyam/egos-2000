#include "egos.h"
#define NMAX_SEGMENTS 5

struct grass *grass = (struct grass *)GRASS_STRUCT_BASE;
struct earth *earth = (struct earth *)EARTH_STRUCT_BASE;

struct segment {
  void *base_vaddr;
  uint rwx, memsz, filesz, fileoff;
};

struct segment_table {
  uint nsegments;
  struct segment segments[NMAX_SEGMENTS];
};

struct segment_table *segtbl = 
  (struct segment_table *) (LOADER_VSTACK_TOP - LOADER_VSTACK_NPAGES * PAGE_SIZE);