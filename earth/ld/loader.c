#include "loader.h"
#include "elf.h"
#include "disk.h"
#include "../../grass/syscall.h"
#include "../../grass/process.h"
#include <string.h>

void *segtbl_init(elf_reader reader) {
  char buf[BLOCK_SIZE];
  reader(0, buf);

  struct elf32_header *header = (void*)buf;
  struct elf32_program_header *pheaders = (void*)buf + header->e_phoff;
  segtbl->nseg = header->e_phnum + 1;

  for (int i = 0; i < header->e_phnum; i++) {
    struct elf32_program_header pheader = pheaders[i];

    segtbl->seg[i].base_vaddr = pheader.p_vaddr;
    segtbl->seg[i].rwx        = pheader.p_flags;
    segtbl->seg[i].memsz      = pheader.p_memsz;
    segtbl->seg[i].filesz     = pheader.p_filesz;
    segtbl->seg[i].fileoff    = pheader.p_offset / BLOCK_SIZE;
  }

  /* Stack Segment */
  segtbl->seg[header->e_phnum].base_vaddr = STACK_VBOTTOM;
  segtbl->seg[header->e_phnum].memsz      = STACK_VTOP - STACK_VBOTTOM;
  return (void *)header->e_entry;
}

int segtbl_find(uint vaddr) {
  for (uint i = 0; i < segtbl->nseg; i++) {
    uint voff = vaddr - segtbl->seg[i].base_vaddr;
    if (voff <= segtbl->seg[i].memsz)
      return i;
  }
  return -1;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void loader_mret(uint mepc, uint *regs);

int load_server(uint block_no, char *dst) {
  grass->sys_disk(SYS_PROC_EXEC_START + block_no, 1, dst, IO_READ);
}

void loader_fault(uint vaddr, uint type) {
  CRITICAL("FAULT: %x, TYPE: %d", vaddr, type);
  uint voff, block_no, vpa = vaddr & ~(0xFFF);
  int seg_idx = segtbl_find(vaddr);

  if (seg_idx == -1) FATAL("SEGFAULT");

  /* Found Segment; Map Vaddr to arbitrary Physical Frame */
  grass->sys_vm_map(vaddr);
  
  /* Load contents into frame */
  if (vaddr - segtbl->seg[seg_idx].base_vaddr <= segtbl->seg[seg_idx].filesz) {
    if ((vpa - (segtbl->seg[seg_idx].base_vaddr)) % PAGE_SIZE)
      FATAL("NOT PAGE ALIGNED: %x, %x", vpa, segtbl->seg[seg_idx].base_vaddr);

    voff = (vpa - (segtbl->seg[seg_idx].base_vaddr)) / BLOCK_SIZE;
    block_no = segtbl->seg[seg_idx].fileoff + voff;
    for (uint p = vpa; p < vpa + PAGE_SIZE; p += BLOCK_SIZE)
      load_server(block_no++, (char *)p);
  } else {
    memset((void*)vpa, 0, PAGE_SIZE);
  }

  /* Return back to User Context */
  struct process *p = (struct process *)LOADER_VSTATE;
  loader_mret(p->mepc, p->saved_register);
}

void loader_init(uint ino) {
  SUCCESS("Initialize GPID PROCESS");

  earth->loader_fault = loader_fault;
  void *start = segtbl_init(load_server);

  SUCCESS("Enter GPID PROCESS");
  asm("mv sp, %0"::"r"(STACK_VTOP));
  asm("jr %0"::"r"(start));
}