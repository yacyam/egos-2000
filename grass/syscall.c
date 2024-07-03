/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: the system call interface to user applications
 */

#include "egos.h"
#include "syscall.h"
#include <string.h>

static struct syscall *sc = (struct syscall*)SYSCALL_ARG;

static void sys_invoke() {
    /* The standard way of system call is using the `ecall` instruction; 
     * Switching to ecall is given to students as an exercise */
    *((int*)MSIP) = 1;
    while (sc->type != SYS_UNUSED);
}

int sys_send(int receiver, char* msg, uint size) {
    if (size > SYSCALL_MSG_LEN) return -1;

    sc->type = SYS_SEND;
    sc->msg.receiver = receiver;
    memcpy(sc->msg.content, msg, size);
    sys_invoke();
    return sc->retval;    
}

int sys_recv(int from, int* sender, char* buf, uint size) {
    if (size > SYSCALL_MSG_LEN) return -1;

    sc->msg.sender = from;
    sc->type = SYS_RECV;
    sys_invoke();
    memcpy(buf, sc->msg.content, size);
    if (sender) *sender = sc->msg.sender;
    return sc->retval;
}

int sys_disk(uint block_no, uint nblocks, char* buf, int rw) {
    void *msg = (void *)sc->msg.content;

    memcpy(msg, &block_no, sizeof(block_no));
    msg += sizeof(block_no);
    memcpy(msg, &nblocks, sizeof(nblocks));
    msg += sizeof(nblocks);
    memcpy(msg, &buf, sizeof(buf));

    sc->type = (rw == IO_READ) ? DISK_READ : DISK_WRITE;
    sys_invoke();

    return sc->retval;
}

int sys_tty(char *buf, uint len, int rw) {
    void *msg = (void *)sc->msg.content;

    memcpy(msg, &buf, sizeof(buf));
    msg += sizeof(buf);
    memcpy(msg, &len, sizeof(len));

    sc->type = (rw == IO_READ) ? TTY_READ : TTY_WRITE;
    sys_invoke();
    return sc->retval;
}

void sys_exit(int status) {
    sc->type = SYS_EXIT;
    sys_invoke();
}

int sys_wait(int *child_pid) {
    memcpy(sc->msg.content, &child_pid, sizeof(child_pid));
    sc->type = SYS_WAIT;
    sys_invoke();
    return sc->retval;
}
