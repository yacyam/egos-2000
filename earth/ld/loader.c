#include "loader.h"
#include "../../grass/syscall.h"
#include "../../grass/process.h"
#include <string.h>

void *segtbl_init(elf_reader reader) {
  char buf[BLOCK_SIZE];
  reader(0, buf);

  struct elf32_header *header = (void*)buf;
  struct elf32_program_header *pheaders = (void*)buf + header->e_phoff;
  segtbl->nseg = header->e_phnum + 1; // +1 (Stack+Heap) Segment

  for (int i = 0; i < header->e_phnum; i++) {
    struct elf32_program_header pheader = pheaders[i];

    segtbl->seg[i].base_vaddr = pheader.p_vaddr;
    segtbl->seg[i].rwx        = pheader.p_flags;
    segtbl->seg[i].memsz      = pheader.p_memsz;
    segtbl->seg[i].filesz     = pheader.p_filesz;
    segtbl->seg[i].fileoff    = pheader.p_offset / BLOCK_SIZE;
  }

  /* Stack + Heap Segment */
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

void loader_ret(uint mepc, uint *regs);

int load_server(uint block_no, char *dst) { grass->sys_disk(elf->offset + block_no, 1, dst, IO_READ); }
int load_app   (uint block_no, char *dst) { file_read(elf->ino, block_no, dst); }

void loader_fault(uint vaddr, uint type) {
  if (sizeof(struct process) + sizeof(struct syscall) > PAGE_SIZE) 
    FATAL("loader_fault: process state overflows 1 page");

  memcpy((void*)LOADER_VSTATE + sizeof(struct process), (void*)SYSCALL_VARG, sizeof(struct syscall));
  //INFO("FAULT: %x, TYPE: %d", vaddr, type);
  uint voff, block_no, vpa = vaddr & ~(0xFFF);
  int seg_idx = segtbl_find(vaddr);

  if (seg_idx == -1) FATAL("SEGFAULT");

  /* Found Segment; Map Vaddr to arbitrary Physical Frame */
  grass->sys_vm_map(vaddr);

  /***
   * TODO: Cannot support multiple segments in one page
   * Ex: One Page contains both data & bss section
   * Simple Solution: Require each segment to be page aligned
   */
  
  /* Load contents into frame */
  if (vaddr - segtbl->seg[seg_idx].base_vaddr <= segtbl->seg[seg_idx].filesz) {
    if (segtbl->seg[seg_idx].base_vaddr % PAGE_SIZE)
      FATAL("NOT PAGE ALIGNED: %x", segtbl->seg[seg_idx].base_vaddr);

    voff = (vpa - (segtbl->seg[seg_idx].base_vaddr)) / BLOCK_SIZE;
    block_no = segtbl->seg[seg_idx].fileoff + voff;
    for (uint p = vpa; p < vpa + PAGE_SIZE; p += BLOCK_SIZE)
      elf->reader(block_no++, (char *)p);
  } else {
    memset((void*)vpa, 0, PAGE_SIZE);
  }

  /* Return back to User Context */
  memcpy((void*)SYSCALL_VARG, (void*)LOADER_VSTATE + sizeof(struct process), sizeof(struct syscall));
  struct process *p = (struct process *)LOADER_VSTATE;
  loader_ret(p->mepc, p->saved_register);
}

void loader_init(int pid, struct proc_args *args) {
  CRITICAL("Loader: Initialize Process %d", pid);
  earth->loader_fault = loader_fault;

  if (pid < GPID_USER_START) {
    switch (pid) {
      case GPID_PROCESS: elf->offset = SYS_PROC_EXEC_START;  break;
      case GPID_FILE:    elf->offset = SYS_FILE_EXEC_START;  break;
      case GPID_DIR:     elf->offset = SYS_DIR_EXEC_START;   break;
      case GPID_SHELL:   elf->offset = SYS_SHELL_EXEC_START; break;
    }
    elf->reader = load_server;
  }
  else {
    /* User */
    elf->ino = args->ino;
    elf->reader = load_app;

    /* Initialize ARGV of User Process */
    void *argv_ptrs  =  (void*)APPS_VARG;
    void *argv_start =  (void*)(APPS_VARG + args->argc * sizeof(void*));
    for (int i = 0; i < args->argc; i++) {
      memcpy(argv_start, &args->argv[i], sizeof(args->argv[i]));
      memcpy(argv_ptrs, &argv_start, sizeof(void*));
      argv_start += sizeof(args->argv[i]);
      argv_ptrs += sizeof(void*);
    }
  }

  void *start = segtbl_init(elf->reader);

  asm("mv ra, %0"::"r"(start));
  asm("mv sp, %0"::"r"(STACK_VTOP));
  asm("mv a0, %0"::"r"((args ? args->argc : 0)));
  asm("mv a1, %0"::"r"(APPS_VARG));
  asm("ret");
}
