/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: system support to C library function malloc()
 */

#include "egos.h"

extern char __heap_start, __heap_end;
static char* brk = &__heap_start;

/* malloc() and free() are linked from the compiler's C library;
 * malloc() and free() manage the memory region [&__heap_start, brk).
 * If malloc() finds this region to be too small, it will call _sbrk().
 */

char *_sbrk(int size) {
    /***
     * TODO: Support arbitrarily large heap
     */
    char* heap_end = (&__heap_end);
    if (brk + size > heap_end) {
        *(int*)(0xFFFFFFF0) = 1; /* Trigger a memory exception */
        // earth->kernel_tty_write("_sbrk: heap grows too large\r\n", 29);
    }

    char *old_brk = brk;
    brk += size;
    return old_brk;
}
