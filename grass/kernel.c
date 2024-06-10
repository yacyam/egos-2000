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
#include "kernel.h"
#include <string.h>

uint proc_curr_idx;
struct process proc_set[MAX_NPROCESS];
struct pending_ipc *msg_buffer;

static void proc_yield();
static void proc_try_syscall(struct process *proc);

void kernel_entry(uint is_interrupt, uint id) {
    /* Save process context */
    asm("csrr %0, mepc" : "=r"(proc_set[proc_curr_idx].mepc));
    memcpy(proc_set[proc_curr_idx].saved_register, SAVED_REGISTER_ADDR, SAVED_REGISTER_SIZE);

    (is_interrupt)? intr_entry(id) : excp_entry(id);

    /* Restore process context */
    asm("csrw mepc, %0" ::"r"(proc_set[proc_curr_idx].mepc));
    memcpy(SAVED_REGISTER_ADDR, proc_set[proc_curr_idx].saved_register, SAVED_REGISTER_SIZE);
}

void excp_entry(uint id) {
    /* Student's code goes here (system call and memory exception). */

    /* If id is for system call, handle the system call and return */

    /* Otherwise, kill the process if curr_pid is a user application */

    /* Student's code ends here. */
    FATAL("excp_entry: kernel got exception %d", id);
}

void intr_entry(uint id) {
    if (id == INTR_ID_TIMER && curr_pid < GPID_SHELL) {
        /* Do not interrupt kernel processes since IO can be stateful */
        earth->timer_reset();
        return;
    }

    if (earth->tty_recv_intr() && curr_pid >= GPID_USER_START) {
        /* User process killed by ctrl+c interrupt */
        INFO("process %d killed by interrupt", curr_pid);
        proc_set[proc_curr_idx].mepc = (uint)sys_exit;
        return;
    }

    /* Ignore other interrupts for now */
    if (id == INTR_ID_SOFT) proc_try_syscall(&proc_set[proc_curr_idx]);
    proc_yield();
}

static void proc_yield() {
    /* Find the next runnable process */
    int next_idx = -1;
    for (uint i = 1; i <= MAX_NPROCESS; i++) {
        struct process *p = &proc_set[(proc_curr_idx + i) % MAX_NPROCESS];
        if (p->status == PROC_PENDING) {
            earth->mmu_switch(p->pid);
            proc_try_syscall(p); // Run pending system call to possibly make process runnable
        }
        if (p->status == PROC_READY || p->status == PROC_RUNNING || p->status == PROC_RUNNABLE) {
            next_idx = (proc_curr_idx + i) % MAX_NPROCESS;
            break;
        }
    }

    if (next_idx == -1) FATAL("proc_yield: no runnable process");
    if (curr_status == PROC_RUNNING) proc_set_runnable(curr_pid);

    /* Switch to the next runnable process and reset timer */
    proc_curr_idx = next_idx;
    earth->mmu_switch(curr_pid);
    earth->timer_reset();

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

static int proc_try_send(struct syscall *sc, struct process *sender) {
    if (msg_buffer->in_use == 1) return -1;
    
    for (uint i = 0; i < MAX_NPROCESS; i++) {
        struct process dst = proc_set[i];
        if (dst.pid == sc->msg.receiver && dst.status != PROC_UNUSED) {
            /* Destination is not receiving, or will not take msg from sender */
            if (! (dst.status == PROC_PENDING && dst.pending_syscall == SYS_RECV) )   return -1;
            if (! (dst.receive_from == GPID_ALL || dst.receive_from == sender->pid) ) return -1;
            
            msg_buffer->in_use = 1;
            msg_buffer->sender = sender->pid;
            msg_buffer->receiver = sc->msg.receiver;

            memcpy(msg_buffer->msg, sc->msg.content, sizeof(sc->msg.content));
            return 0;
        }
    }
    FATAL("proc_try_send: process %d sending to unknown process %d", sender->pid, sc->msg.receiver);
}

static int proc_try_recv(struct syscall *sc, struct process *receiver) {
    receiver->receive_from = sc->msg.sender;
    
    /* No Message Available, or not for Current Process */
    if (msg_buffer->in_use == 0 || msg_buffer->receiver != receiver->pid) return -1; 

    memcpy(sc->msg.content, msg_buffer->msg, sizeof(sc->msg.content));
    sc->msg.sender = msg_buffer->sender;

    msg_buffer->in_use = 0;
    return 0;
}

static void proc_try_syscall(struct process *proc) {
    struct syscall *sc = (struct syscall*)SYSCALL_ARG;
    int rc;

    *((int*)MSIP) = 0;

    switch (sc->type) {
    case SYS_RECV:
        rc = proc_try_recv(sc, proc);
        break;
    case SYS_SEND:
        rc = proc_try_send(sc, proc);
        break;
    default:
        FATAL("proc_try_syscall: got unknown syscall type=%d", sc->type);
    }

    if (rc == -1) {
        proc_set_pending(proc->pid); // Failure, Retry Request
        proc->pending_syscall = sc->type;
    } else {
        proc_set_runnable(proc->pid); // Success or Error, Move to User Space
        sc->type = SYS_UNUSED;
    }
}
