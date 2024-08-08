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
#include "string.h"

#define SEC_DATA  "data"
#define SEC_HEAP  "heap"
#define SEC_STACK "stack"

#define DIFF_EASY       "light"
#define DIFF_MED        "moderate"
#define DIFF_HARD       "hard"
#define DIFF_IMPOSSIBLE "impossible"
#define DIFF_RANDOM     "random"

#define LARGE_NUMBER 100000000
#define RAND_NTEST 100

char data[LARGE_NUMBER];

int sum(int i) {
    if (i == 0) return 0;
    return i + sum(i - 1);
}

void memtest(char *buf, uint len) {
    char special = 'y';
    for (int i = 0; i < len; i++) 
        buf[i] = special;
    for (int i = 0; i < len; i++)
        if (buf[i] != special)
            FATAL("memtest: failed at byte %d", i);
}

void test_data(char *diff) {
    CRITICAL("Data Test Start");
    
    if (!strcmp(diff, DIFF_EASY))
        memtest(data, 100);
    if (!strcmp(diff, DIFF_MED))
        memtest(data, 10000);
    if (!strcmp(diff, DIFF_HARD))
        memtest(data, 100000);
    if (!strcmp(diff, DIFF_IMPOSSIBLE))
        memtest(data, LARGE_NUMBER);
    if (!strcmp(diff, DIFF_RANDOM))
        for (int i = 0; i < RAND_NTEST; i++)
            memtest(data + (rand() % LARGE_NUMBER), 1);
    
    CRITICAL("Data Test End");
}

void test_heap() {
    FATAL("UNIMPLEMENTED");
}

void test_stack(char *diff) {
    CRITICAL("Stack Test Start");

    if (!strcmp(diff, DIFF_EASY))
        sum(100);
    if (!strcmp(diff, DIFF_MED))
        sum(1000);
    if (!strcmp(diff, DIFF_HARD))
        sum(20000);
    if (!strcmp(diff, DIFF_IMPOSSIBLE))
        sum(100000);
    if (!strcmp(diff, DIFF_RANDOM)) {
        uint sp;
        asm("mv %0, sp":"=r"(sp));
        for (int i = 0; i < RAND_NTEST; i++) 
            memtest((void*)sp - PAGE_SIZE - (rand() % LARGE_NUMBER), 1);
    }

    CRITICAL("Stack Test Done");
}

int main(int argc, void **argv) {
    if (argc != 3) {
        printf("USAGE: crash1 [section] [difficulty]\n");
        printf("[section]:    %s, %s, %s\n", SEC_DATA, SEC_HEAP, SEC_STACK);
        printf("[difficulty]: %s, %s, %s, %s, %s\n", DIFF_EASY, DIFF_MED, DIFF_HARD, DIFF_IMPOSSIBLE, DIFF_RANDOM);
        return -1;
    }

    if (!strcmp(argv[1], "data"))
        test_data(argv[2]);
    if (!strcmp(argv[1], "heap"))
        test_heap();
    if (!strcmp(argv[1], "stack"))
        test_stack(argv[2]);
}
