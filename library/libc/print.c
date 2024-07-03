/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: system support to C library function printf()
 */

#include "egos.h"
#include <unistd.h>

/* printf() is linked from the compiler's C library;
 * printf() constructs a string based on its arguments
 * and prints the string to the tty device by calling _write().
 */

int _write(int file, char *ptr, uint len) {
    if (file != STDOUT_FILENO) return -1;

    if (grass->mode == MODE_KERNEL) 
        return earth->kernel_tty_write(ptr, len);

    if (grass->sys_tty(ptr, len, IO_WRITE) == -1) {
        _write(file, ptr, len / 2);
        _write(file, ptr + (len / 2), len - (len / 2));
    }

    return 0;
}

int _close(int file) { return -1; }
int _fstat(int file, void *stat) { return -1; }
int _lseek(int file, int ptr, int dir) { return -1; }
int _read(int file, void *ptr, uint len) { return -1; }
int _isatty(int file) { return (file == STDOUT_FILENO); }
void _kill() {}
int _getpid() { return -1; }
void _exit(int status) { grass->sys_exit(status); while(1); }
