/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: grass layer initialization
 * Initialize timer and the process control block; 
 * Spawn the first kernel process, GPID_PROCESS (pid=1).
 */

#include "egos.h"
#include "syscall.h"
#include "process.h"

struct grass *grass = (void*)GRASS_STRUCT_BASE;
struct earth *earth = (void*)EARTH_STRUCT_BASE;

static int loader_read(uint block_no, char* dst) {
    earth->kernel_disk_read(LOADER_EXEC_START + block_no, 1, dst);
    return 0;
}

int main() {
    CRITICAL("Enter the grass layer");
    void *sc;

    /* Initialize the grass interface functions */
    grass->proc_alloc = proc_alloc;
    grass->proc_free = proc_free;
    grass->proc_set_ready = proc_set_ready;

    grass->sys_wait = sys_wait;
    grass->sys_exit = sys_exit;
    grass->sys_send = sys_send;
    grass->sys_recv = sys_recv;
    grass->sys_disk = sys_disk;
    grass->sys_tty  = sys_tty;

    /* Register the kernel entry */
    earth->kernel_entry_init(kernel_entry);

    /* Initialize IPC Buffer */
    pending_ipc_buffer = (struct pending_ipc *)((uint)grass + sizeof(*grass));
    pending_ipc_buffer->in_use = 0;
    
    /* Load the first kernel process GPID_PROCESS */
    INFO("Load kernel process #%d: sys_proc", GPID_PROCESS);
    elf_load(GPID_PROCESS, loader_read, 0, 0);

    earth->mmu_alloc(GPID_PROCESS, &sc);
    earth->mmu_switch(GPID_PROCESS);
    proc_set_running(proc_alloc(GPID_UNUSED, sc));

    CRITICAL("Enter GPID_PROCESS's Loader");
    grass->mode = MODE_USER;
    /* Jump to the entry of process GPID_PROCESS's Loader */
    asm("csrw mepc, %0" ::"r"(LOADER_PENTRY));
    asm("mv sp, %0"::"r"(LOADER_VSTACK_TOP));
    asm("mret");
}
