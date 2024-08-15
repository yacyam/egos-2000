/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: memory management unit (MMU)
 * implementation of 2 translation mechanisms: page table and software TLB
 */

#include "egos.h"
#include "disk.h"
#include "servers.h"
#include <string.h>

#define PINNED    1
#define UNPINNED  0

#define PAGE_ID_TO_PADDR(i) (CORE_MAP_START + (i * PAGE_SIZE))

struct frame {
  int in_use, pid, pinned;
};

/* Physical Pages */
struct frame core_map[CORE_MAP_NPAGES];

char *frame_acquire(int pid, int pinned) {
    int free_idx = -1;

    for (int i = 0; i < CORE_MAP_NPAGES; i++)
        if (!core_map[i].in_use) {
            free_idx = i;
            break;
        }

    if (free_idx == -1) FATAL("acquire_frame: ran out of free frames");

    core_map[free_idx].in_use = 1;
    core_map[free_idx].pid = pid;
    core_map[free_idx].pinned = pinned;

    /* Ith core map entry corresponds to Ith page in RAM */
    return (char *)PAGE_ID_TO_PADDR(free_idx);
}

void frame_flush(int pid) {
    for (int i = 0; i < CORE_MAP_NPAGES; i++)
        if (core_map[i].in_use && core_map[i].pid == pid) {
            core_map[i].in_use = 0;
            /* Must 0 Out for Page Table Pages */
            memset((void*)PAGE_ID_TO_PADDR(i), 0, PAGE_SIZE);
        }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define VPN_NBITS    0x3FF
#define VPN_SIZE     10
#define PPN_SIZE     22
#define PAGE_OFF     12
#define PAGE_NLEVELS 2

#define RWX          0b1110

typedef uint pte;
struct _page_table {
  uint table[PAGE_SIZE / sizeof(uint)];
} typedef page_table;

/* Page Tables: 2 Levels */
page_table *pid_root_pagetable[32]; /* Points to Root PT */

/* Update PTE from pagetable, or create new PTE if invalid */
pte pagetable_update_pte(int pid, page_table *ptbl, uint vpn, uint ppn, int rwx, int pin) {
    pte pte = ptbl->table[vpn];

    /* Don't care where PTE is mapped, reuse pte or acquire new frame */
    if (ppn == (uint)NULL) {
        if (pte & 0x1) ppn = pte >> 10;
        else           ppn = (uint)frame_acquire(pid, pin) >> 12;
    }

    ptbl->table[vpn] = (ppn << 10) | rwx | 0x1;
    return ptbl->table[vpn];
}

/* 
    pagetable_map: 
        Maps VADDR to PADDR in PID's Page Table. 
        If PADDR == NULL, map VADDR to any page.
*/
char *pagetable_map(int pid, uint vaddr, uint paddr, int rwx, int pin) {
    uint vpn1 = ((uint)vaddr >> 22) & VPN_NBITS;
    uint vpn0 = ((uint)vaddr >> 12) & VPN_NBITS;
    uint ppn  = ((uint)paddr >> 12);

    page_table *leaf, *root = pid_root_pagetable[pid];
    char *page;

    if (root == 0)
        pid_root_pagetable[pid] = root = (page_table *)frame_acquire(pid, pin);


    leaf = (page_table *)((pagetable_update_pte(pid, root, vpn1,   0,   0, pin) << 2) & ~(0xFFF));
    page =       (char *)((pagetable_update_pte(pid, leaf, vpn0, ppn, rwx, pin) << 2) & ~(0xFFF));

    return page;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void mmu_alloc(int pid) {
    /* Map Loader Process' Pages */
    pagetable_map(pid, LOADER_PENTRY,             LOADER_PENTRY,             RWX, PINNED);
    pagetable_map(pid, LOADER_PENTRY + PAGE_SIZE, LOADER_PENTRY + PAGE_SIZE, RWX, PINNED);
    pagetable_map(pid, LOADER_VSTATE, (uint)NULL, RWX, PINNED);
    for (
        uint p = LOADER_VSTACK_TOP - (LOADER_VSTACK_NPAGES * PAGE_SIZE); 
        p < LOADER_VSTACK_TOP; 
        p += PAGE_SIZE
    ) {
        pagetable_map(pid, p, (uint)NULL, RWX, PINNED);
    }

    /* Map Syscall (args + grass/earth struct), Args, and OS */
    pagetable_map(pid, SYSCALL_VARG, (uint)NULL, RWX, PINNED);
    pagetable_map(pid, GRASS_STRUCT_BASE, GRASS_STRUCT_BASE, RWX, PINNED);
    pagetable_map(pid, EARTH_STRUCT_BASE, EARTH_STRUCT_BASE, RWX, PINNED);

    pagetable_map(pid, APPS_VARG,    (uint)NULL, RWX, PINNED);

    /* OS */
    for (uint p = EARTH_ENTRY; p < EARTH_ENTRY + GRASS_SIZE + EARTH_SIZE; p += PAGE_SIZE)
        pagetable_map(pid, p, p, RWX, PINNED);

    for (uint p = ROM_START; p < ROM_START + ROM_SIZE; p += PAGE_SIZE)
        pagetable_map(pid, p, p, RWX, PINNED);
}

void mmu_switch(int pid) {
    if (pid >= 32) FATAL("mmu_switch::: pid_root_pagetable full");

    uint satp = 
          (1 << 31)   /* Mode = Sv32 */
        | (pid << 22) /* ASID = PID  */
        | ((uint)pid_root_pagetable[pid] >> 12); /* PPN */
    asm("csrw satp, %0"::"r" (satp));
}

void mmu_free(int pid) {
    frame_flush(pid);
}

char *mmu_find(int pid, uint vaddr) {
    return pagetable_map(pid, vaddr, (uint)NULL, RWX, UNPINNED);
}

int mmu_map(int pid, uint vaddr) {
    pagetable_map(pid, vaddr, (uint)NULL, RWX, UNPINNED);
    return 0;
}

/* MMU Initialization */
void mmu_init() {
    /* Instruction Access Fault on Virtual Memory without PMPCFG0 Set */
    asm("csrw pmpaddr0, %0" : : "r" (0x40000000));
    asm("csrw pmpcfg0, %0" : : "r" (0xF));
    
    earth->mmu_alloc = mmu_alloc;
    earth->mmu_switch = mmu_switch;
    earth->mmu_find = mmu_find;
    earth->mmu_free = mmu_free;
    earth->mmu_map  = mmu_map;
}
