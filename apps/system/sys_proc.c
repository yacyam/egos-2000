/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: system process for spawning and killing other processes
 */

#include "app.h"
#include "elf.h"
#include "disk.h"
#include <string.h>

static int app_ino, app_pid;
static void sys_spawn(uint base);
static int app_spawn(struct proc_request *req, int parent);

int main() {
    SUCCESS("Enter kernel process GPID_PROCESS");

    int sender, shell_waiting;
    char buf[SYSCALL_MSG_LEN];

    buf[0] = 'y';
    grass->sys_disk(0x08, 1, buf, 1);
    grass->sys_disk(0x08, 1, buf, 0);

    SUCCESS("CHAR %c", buf[0]);

    while(1);

    sys_spawn(SYS_FILE_EXEC_START);
    grass->sys_recv(GPID_FILE, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_proc receives: %s", buf);

    sys_spawn(SYS_DIR_EXEC_START);
    grass->sys_recv(GPID_DIR, NULL, buf, SYSCALL_MSG_LEN);
    INFO("sys_proc receives: %s", buf);

    sys_spawn(SYS_SHELL_EXEC_START);
    
    while (1) {
        struct proc_request *req = (void*)buf;
        struct proc_reply *reply = (void*)buf;
        grass->sys_recv(GPID_ALL, &sender, buf, SYSCALL_MSG_LEN);

        switch (req->type) {
        case PROC_SPAWN:
            /* Handling background processes */
            shell_waiting = (req->argv[req->argc - 1][0] != '&');

            reply->type = app_spawn(req, sender) < 0 ? CMD_ERROR : CMD_OK;
            reply->pid = app_pid;
            grass->sys_send(sender, (void*)reply, sizeof(*reply));
            break;
        case PROC_KILLALL:
            grass->proc_free(GPID_ALL); break;
        default:
            FATAL("sys_proc: invalid request %d", req->type);
        }
    }
}

static int app_read(uint off, char* dst) { file_read(app_ino, off, dst); }

static int app_spawn(struct proc_request *req, int parent) {
    int bin_ino = dir_lookup(0, "bin/");
    if ((app_ino = dir_lookup(bin_ino, req->argv[0])) < 0) return -1;

    app_pid = grass->proc_alloc(parent);

    if (app_pid < 0) {
        grass->proc_free(GPID_ALL);
        app_pid = grass->proc_alloc(parent);
        if (app_pid < 0) FATAL("Reached Maximum Number of Processes");
    } 

    int argc = req->argv[req->argc - 1][0] == '&'? req->argc - 1 : req->argc;

    elf_load(app_pid, app_read, argc, (void**)req->argv);
    SUCCESS("SPAWNED");
    grass->proc_set_ready(app_pid);
    return 0;
}

static int sys_proc_base;
char* sysproc_names[] = {"sys_proc", "sys_file", "sys_dir", "sys_shell"};

static int sys_proc_read(uint block_no, char* dst) {
    return grass->sys_disk(sys_proc_base + block_no, 1, dst, 0);
}

static void sys_spawn(uint base) {
    int pid = grass->proc_alloc(GPID_PROCESS);
    INFO("Load kernel process #%d: %s", pid, sysproc_names[pid - 1]);

    sys_proc_base = base;
    elf_load(pid, sys_proc_read, 0, NULL);
    grass->proc_set_ready(pid);
}
