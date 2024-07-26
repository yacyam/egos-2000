#include "loader.h"
#include "elf.h"
#include "disk.h"

int load_server(uint block_no, char *dst) {
  grass->sys_disk(SYS_PROC_EXEC_START + block_no, 1, dst, IO_READ);
}

void *init_segtbl(elf_reader reader) {
  char buf[BLOCK_SIZE];
  reader(0, buf);

  struct elf32_header *header = (void*)buf;
  struct elf32_program_header *pheaders = (void*)buf + header->e_phoff;
  segtbl->nsegments = header->e_phnum + 1;

  for (int i = 0; i < header->e_phnum; i++) {
    struct elf32_program_header pheader = pheaders[i];

    segtbl->segments[i].base_vaddr = (void*)pheader.p_vaddr;
    segtbl->segments[i].rwx        = pheader.p_flags;
    segtbl->segments[i].memsz      = pheader.p_memsz;
    segtbl->segments[i].filesz     = pheader.p_filesz;
    segtbl->segments[i].fileoff    = pheader.p_offset;
  }

  /* Stack Segment */
  segtbl->segments[header->e_phnum].base_vaddr = (void*)STACK_VBOTTOM;
  segtbl->segments[header->e_phnum].memsz      = STACK_VTOP - STACK_VBOTTOM;
  return (void *)header->e_entry;
}

void loader_fault(uint vaddr, uint type) {
  FATAL("LOADER FAULT");
}

void loader_init(uint ino) {
  SUCCESS("Initialize GPID PROCESS");

  earth->loader_fault = loader_fault;
  void *start = init_segtbl(load_server);
  SUCCESS("Enter GPID PROCESS");
  asm("jr %0"::"r"(start));
}