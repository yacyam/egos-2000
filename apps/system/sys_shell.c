/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a simple shell
 */

#include "app.h"
#include <string.h>

int read_chars(char *buf, int len) {
    char c;

    for (int i = 0; i < len - 1; i++) {
        grass->sys_tty(&c, 1, IO_READ);
        buf[i] = (char)c;

        switch (c) {
        case 0x03: /* Ctrl+C    */
            buf[0] = 0;
        case 0x0d: /* Enter     */
            buf[i] = 0;
            printf("\r\n");
            return c == 0x03 ? 0 : i;
        case 0x7f: /* Backspace */
            c = 0;
            if (i) printf("\b \b");
            i = i ? i - 2 : i - 1;
        }

        if (c) printf("%c", c);
    }

    buf[len - 1] = 0;
    return len - 1;
}

int parse_request(char* buf, struct proc_request* req) {
    uint idx = 0, nargs = 0;
    memset(req->argv, 0, CMD_NARGS * CMD_ARG_LEN);

    for (uint i = 0; i < strlen(buf); i++)
        if (buf[i] != ' ') {
            req->argv[nargs][idx] = buf[i];
            if (++idx >= CMD_ARG_LEN) return -1;
        } else if (idx != 0) {
            idx = req->argv[nargs][idx] = 0;
            if (++nargs >= CMD_NARGS) return -1;
        }
    req->argc = idx ? nargs + 1 : nargs;
    return 0;
}

int main() {
    CRITICAL("Welcome to the egos-2000 shell!");
    
    char buf[256] = "cd";  /* Enter the home directory first */
    while (1) {
        struct proc_request req;
        struct proc_reply reply;

        if (strcmp(buf, "killall") == 0) {
            req.type = PROC_KILLALL;
            grass->sys_send(GPID_PROCESS, (void*)&req, sizeof(req));
        } else {
            req.type = PROC_SPAWN;

            if (0 != parse_request(buf, &req)) {
                INFO("sys_shell: too many arguments or argument too long");
            } else {
                grass->sys_send(GPID_PROCESS, (void*)&req, sizeof(req));
                grass->sys_recv(GPID_PROCESS, NULL, (void*)&reply, sizeof(reply));

                if (reply.type != CMD_OK)
                    INFO("sys_shell: command causes an error");
                else if (req.argv[req.argc - 1][0] == '&')
                    INFO("process %d running in the background", reply.pid);
                else {
                    /* Wait for foreground command to terminate */
                    int child_pid, foreground_pid = reply.pid;
                    do {
                        grass->sys_wait(&child_pid);
                        if (child_pid != foreground_pid) INFO("background process %d terminated", child_pid);
                    } while (child_pid != foreground_pid);
                }
            }
        }

        do {
            printf("\x1B[1;32m➜ \x1B[1;36m%s\x1B[1;0m ", grass->workdir);
            while (1);
        } while (read_chars(buf, 256) == 0);
    }
}
