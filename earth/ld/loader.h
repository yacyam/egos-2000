#include "egos.h"
#include "elf.h"
#include "disk.h"
#define MAX_NSEGMENTS 5

struct grass *grass = (struct grass *)GRASS_STRUCT_BASE;
struct earth *earth = (struct earth *)EARTH_STRUCT_BASE;

struct segment {
  uint base_vaddr, rwx, memsz, filesz, fileoff;
};

struct segment_table {
  uint nseg;
  struct segment seg[MAX_NSEGMENTS];
};

struct elf_data {
  elf_reader reader;
  uint offset, ino;
};

struct segment_table *segtbl = (struct segment_table *)(LOADER_VSTACK_BOTTOM);
struct elf_data *elf = (struct elf_data *)(LOADER_VSTACK_BOTTOM + sizeof(*segtbl));