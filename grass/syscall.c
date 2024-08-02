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

static struct syscall *sc = (struct syscall*)SYSCALL_VARG;

static void sys_invoke() {
    asm("ecall");
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
    sc->type = (rw == IO_READ) ? DISK_READ : DISK_WRITE;
    memcpy(sc->args.argv[0], &block_no, sizeof(block_no));
    memcpy(sc->args.argv[1], &nblocks,  sizeof(nblocks));
    sys_invoke();
    memcpy(buf, sc->msg.content, BLOCK_SIZE);
    return sc->retval;
}

int sys_tty(char *buf, uint len, int rw) {
    sc->type = (rw == IO_READ) ? TTY_READ : TTY_WRITE;
    if (sc->type == TTY_WRITE) memcpy(sc->msg.content,  buf, len);
    memcpy(sc->args.argv[0], &len, sizeof(len));
    sys_invoke();
    memcpy(buf, sc->msg.content, len);
    return sc->retval;
}

void sys_exit(int status) {
    sc->type = SYS_EXIT;
    sys_invoke();
}

int sys_wait(int *child_pid) {
    sc->type = SYS_WAIT;
    sys_invoke();
    memcpy(child_pid, sc->args.argv[0], sizeof(*child_pid));
    return sc->retval;
}

void sys_vm_map(uint vaddr) {
    memcpy(sc->args.argv[0], &vaddr, sizeof(vaddr));
    sc->type = VM_MAP;
    sys_invoke();
}
