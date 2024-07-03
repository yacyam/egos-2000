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

struct grass *grass = (void*)APPS_STACK_TOP;
struct earth *earth = (void*)GRASS_STACK_TOP;

static int sys_proc_read(uint block_no, char* dst) {
    earth->kernel_disk_read(SYS_PROC_EXEC_START + block_no, 1, dst);
    return 0;
}

int main() {
    CRITICAL("Enter the grass layer");

    /* Initialize the grass interface functions */
    grass->proc_alloc = proc_alloc;
    grass->proc_free = proc_free;
    grass->proc_set_ready = proc_set_ready;

    grass->sys_wait = sys_wait;
    grass->sys_exit = sys_exit;
    grass->sys_send = sys_send;
    grass->sys_recv = sys_recv;
    grass->sys_disk = sys_disk;

    /* Register the kernel entry */
    earth->kernel_entry_init(kernel_entry);

    /* Initialize IPC Buffer */
    pending_ipc_buffer = (struct pending_ipc *)((uint)grass + sizeof(*grass));
    pending_ipc_buffer->in_use = 0;
    
    /* Load the first kernel process GPID_PROCESS */
    INFO("Load kernel process #%d: sys_proc", GPID_PROCESS);
    elf_load(GPID_PROCESS, sys_proc_read, 0, 0);
    proc_set_running(proc_alloc(GPID_UNUSED));
    earth->mmu_switch(GPID_PROCESS);

    grass->mode = MODE_USER;
    /* Jump to the entry of process GPID_PROCESS */
    asm("mv a0, %0" ::"r" (APPS_ARG));
    asm("csrw mepc, %0" ::"r"(APPS_ENTRY));
    asm("mret");
}
