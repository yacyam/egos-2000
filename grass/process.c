/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: helper functions for managing processes
 */

#include "egos.h"
#include "syscall.h"
#include "process.h"
#include <string.h>

static void proc_set_status(int pid, enum proc_status status) {
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid == pid) proc_set[i].status = status;
}

void proc_set_ready(int pid) { proc_set_status(pid, PROC_READY); }
void proc_set_running(int pid) { proc_set_status(pid, PROC_RUNNING); }
void proc_set_runnable(int pid) { proc_set_status(pid, PROC_RUNNABLE); }
void proc_set_pending(int pid) { proc_set_status(pid, PROC_PENDING); }
void proc_set_zombie(int pid) { proc_set_status(pid, PROC_ZOMBIE); }

int proc_alloc(int parent_pid, void *sc) {
    static uint proc_nprocs = 0;
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].status == PROC_UNUSED) {
            proc_set[i].pid = ++proc_nprocs;
            proc_set[i].status = PROC_LOADING;
            proc_set[i].parent_pid = parent_pid;

            proc_set[i].sc = (struct syscall *)sc;
            return proc_nprocs;
        }

    return -1;
}

void proc_free(int pid) {
    if (pid != GPID_ALL) {
        earth->mmu_free(pid);
        proc_set_status(pid, PROC_UNUSED);
        return;
    }

    /* Free all user applications */
    for (uint i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].pid >= GPID_USER_START &&
            proc_set[i].status != PROC_UNUSED) {
            earth->mmu_free(proc_set[i].pid);
            proc_set[i].status = PROC_UNUSED;
        }
}
