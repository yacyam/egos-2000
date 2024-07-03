#pragma once

#include "servers.h"

enum syscall_type {
	SYS_UNUSED,
	SYS_RECV,
	SYS_SEND,
    SYS_EXIT,
    SYS_WAIT,
    DISK_READ,
    DISK_WRITE,
    TTY_READ,
    TTY_WRITE,
	SYS_NCALLS
};

struct sys_msg {
    int sender;
    int receiver;
    char content[SYSCALL_MSG_LEN];
};

struct syscall {
    enum syscall_type type;  /* Type of the system call */
    struct sys_msg msg;      /* Data of the system call */
    int retval;              /* Return value of the system call */
};

struct pending_ipc
{
    int in_use;
    int sender;
    int receiver;
    char msg[SYSCALL_MSG_LEN];
};

extern struct pending_ipc *pending_ipc_buffer;

void sys_exit(int status);
int  sys_wait(int *pid);
int  sys_send(int pid, char* msg, uint size);
int  sys_recv(int pid, int* sender, char* buf, uint size);
int  sys_disk(uint block_no, uint nblocks, char* dst, int rw);
int  sys_tty (char *buf, uint len, int rw);
