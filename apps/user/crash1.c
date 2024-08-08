/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a program causing a memory exception
 * Students are asked to modify the grass kernel so that this 
 * program terminates gracefully without crashing the grass kernel.
 */

#include "app.h"
#include <stdlib.h>

#define LARGE_NUMBER 10000

const char *array = "HELLO WORLD!!!";

int sum(int i) {
    if (i == 0) return 0;
    return i + sum(i - 1);
}

int main(int argc, void **argv) {
    int num = 1;
    char buf[1];
    int s = sum(argc > 1 ? num : 1000);
    //CRITICAL("SUM: %d", s);
    int i = 0;
    while (i++ < 1000000000);
    grass->sys_tty(buf, 1, IO_READ);
    CRITICAL("ARRAY: %s", array);
    //char* heap_overflow = malloc(32 * 1024 * 1024);
}
