/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: Kernel ≈ 3 handlers
 *     proc_yield() handles timer interrupt for process scheduling
 *     excp_entry() handles faults such as unauthorized memory access
 *     proc_syscall() handles system calls for inter-process communication
 */


#include "egos.h"
#include "syscall.h"
#include "process.h"
#include <string.h>

void kernel_entry(uint is_interrupt, uint id) {
    /* Save process context */
    asm("csrr %0, mepc" : "=r"(proc_set[proc_curr_idx].mepc));
    memcpy(proc_set[proc_curr_idx].saved_register, SAVED_REGISTER_ADDR, SAVED_REGISTER_SIZE);

    (is_interrupt)? intr_entry(id) : excp_entry(id);

    /* Restore process context */
    asm("csrw mepc, %0" ::"r"(proc_set[proc_curr_idx].mepc));
    memcpy(SAVED_REGISTER_ADDR, proc_set[proc_curr_idx].saved_register, SAVED_REGISTER_SIZE);
}

#define EXCP_ID_ECALL_U    8
#define EXCP_ID_ECALL_M    11

void excp_entry(uint id) {
    /* Student's code goes here (system call and memory exception). */

    /* If id is for system call, handle the system call and return */

    /* Otherwise, kill the process if curr_pid is a user application */

    /* Student's code ends here. */
    FATAL("exc %d", id);
}

#define INTR_ID_SOFT       3
#define INTR_ID_TIMER      7
#define INTR_ID_EXTR       11

static void proc_yield();
static void proc_syscall(struct process *proc);
static void proc_external();

uint proc_curr_idx;
struct process proc_set[MAX_NPROCESS];
struct pending_ipc *pending_ipc_buffer;

void intr_entry(uint id) {
    if (id == INTR_ID_TIMER && curr_pid < GPID_SHELL) {
        /* Do not interrupt kernel processes since IO can be stateful */
        earth->timer_reset();
        return;
    }

    /* Ignore other interrupts for now */
    if (id == INTR_ID_SOFT) proc_syscall(&proc_set[proc_curr_idx]);
    if (id == INTR_ID_EXTR) proc_external();
    proc_yield();
}

static void proc_external() {
    earth->trap_external();
}

static void proc_wfi() {
    uint mie;
    asm("csrr %0, mie":"=r"(mie));

    asm("csrw mie, %0" ::"r"(mie & ~(0x80))); // Turn off timer interrupts
    asm("wfi");
    asm("csrw mie, %0" ::"r"(mie | 0x80)); // Turn on timer interrupts

    proc_external();
}

static void proc_yield() {
    /* Find the next runnable process */
    int next_idx = -1;
    while (next_idx == -1) {
        for (uint i = 1; i <= MAX_NPROCESS; i++) {
            struct process *p = &proc_set[(proc_curr_idx + i) % MAX_NPROCESS];
            if (p->status == PROC_PENDING) {
                earth->mmu_switch(p->pid);
                proc_syscall(p); // Run pending system call to possibly make process runnable
            }
            if (p->status == PROC_READY || p->status == PROC_RUNNING || p->status == PROC_RUNNABLE) {
                next_idx = (proc_curr_idx + i) % MAX_NPROCESS;
                break;
            }
        }

        if (next_idx == -1) 
            proc_wfi();
    }

    if (curr_status == PROC_RUNNING) proc_set_runnable(curr_pid);

    /* Switch to the next runnable process and reset timer */
    proc_curr_idx = next_idx;
    earth->mmu_switch(curr_pid);
    earth->timer_reset();

    /* Turn off timer if Kernel Process */
    if (curr_pid < GPID_SHELL) earth->timer_disable();
    else earth->timer_enable();

    /* Student's code goes here (switch privilege level). */

    /* Modify mstatus.MPP to enter machine or user mode during mret
     * depending on whether curr_pid is a grass server or a user app
     */

    /* Student's code ends here. */

    /* Call the entry point for newly created process */
    if (curr_status == PROC_READY) {
        /* Set argc, argv and initial program counter */
        proc_set[proc_curr_idx].saved_register[8] = APPS_ARG;
        proc_set[proc_curr_idx].saved_register[9] = APPS_ARG + 4;
        proc_set[proc_curr_idx].mepc = APPS_ENTRY;
    }

    proc_set_running(curr_pid);
}

static int proc_send(struct syscall *sc, struct process *sender) {
    if (pending_ipc_buffer->in_use == 1) return -1;
    
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process dst = proc_set[i];
        if (dst.pid == sc->msg.receiver) {
            /* Destination is not receiving, or will not take msg from sender */
            if (! (dst.status == PROC_PENDING && dst.pending_syscall == SYS_RECV) ) return -1;
            if (! (dst.receive_from == GPID_ALL || dst.receive_from == sender->pid) ) return -1;
            
            pending_ipc_buffer->in_use = 1;
            pending_ipc_buffer->sender = sender->pid;
            pending_ipc_buffer->receiver = sc->msg.receiver;

            memcpy(pending_ipc_buffer->msg, sc->msg.content, sizeof(sc->msg.content));
            return 0;
        }
    }

    return -2; // Error, receiver does not exist
}

static int proc_recv(struct syscall *sc, struct process *receiver) {
    receiver->receive_from = sc->msg.sender;
    
    /* No Message Available, or not for Current Process */
    if (pending_ipc_buffer->in_use == 0 || pending_ipc_buffer->receiver != receiver->pid) 
        return -1; 

    memcpy(sc->msg.content, pending_ipc_buffer->msg, sizeof(sc->msg.content));
    sc->msg.sender = pending_ipc_buffer->sender;

    pending_ipc_buffer->in_use = 0;
    return 0;
}

static int proc_wait(struct syscall *sc, struct process *proc) {
    int *child_pid;
    memcpy(&child_pid, sc->msg.content, sizeof(child_pid));

    for (int i = 0; i < MAX_NPROCESS; i++) {
        if (proc_set[i].parent_pid == proc->pid && proc_set[i].status == PROC_ZOMBIE) {
            *child_pid = proc_set[i].pid;
            proc_free(proc_set[i].pid);
            return 0;    
        }
    }

    return -1;
}

static void proc_exit(struct process *proc) {
    proc_set_zombie(proc->pid);

    for (int i = 0; i < MAX_NPROCESS; i++)
        if (proc_set[i].parent_pid == proc->pid) 
            proc_set[i].parent_pid = proc->parent_pid; // Our Parent becomes Children's parent
}

static int proc_disk(struct syscall *sc) {
    void *msg = (void *)sc->msg.content;

    uint block_no, nblocks;
    char *buf;

    memcpy(&block_no, msg, sizeof(block_no));
    msg += sizeof(block_no);
    memcpy(&nblocks, msg, sizeof(nblocks));
    msg += sizeof(nblocks);
    memcpy(&buf, msg, sizeof(buf));

    if (sc->type == DISK_READ)
        return earth->disk_read(block_no, nblocks, buf);

    return earth->disk_write(block_no, nblocks, buf);
}

static int proc_tty(struct syscall *sc) {
    void *msg = (void *)sc->msg.content;
    char *buf; uint len;

    memcpy(&buf, msg, sizeof(buf));
    msg += sizeof(buf);
    memcpy(&len, msg, sizeof(len));

    if (sc->type == TTY_READ)
        return earth->tty_read(buf);
    return earth->tty_write(buf, len);
}

static void proc_syscall(struct process *proc) {
    struct syscall *sc = (struct syscall*)SYSCALL_ARG;
    int rc;

    *((int*)MSIP) = 0;

    switch (sc->type) {
    case SYS_RECV:
        rc = proc_recv(sc, proc);
        break;
    case SYS_SEND:
        rc = proc_send(sc, proc);
        break;
    case SYS_WAIT:
        rc = proc_wait(sc, proc);
        break;
    case SYS_EXIT:
        proc_exit(proc);
        return;
    case DISK_READ: case DISK_WRITE:
        rc = proc_disk(sc);
        break;
    case TTY_READ: case TTY_WRITE:
        rc = proc_tty(sc);
        break;
    default:
        FATAL("proc_syscall: got unknown syscall type=%d", sc->type);
    }

    if (rc == -1) {
        proc_set_pending(proc->pid); // Failure, Retry Request
        proc->pending_syscall = sc->type;
    } else {
        proc_set_runnable(proc->pid); // Success or Error, Move to User Space
        sc->type = SYS_UNUSED;
    }
    
    sc->retval = (rc == 0) ? 0 : -1;
}
