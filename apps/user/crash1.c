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
#include "mem.h"

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

void memwrite(char *buf, uint len, char special) {
    for (int i = 0; i < len; i++) 
        buf[i] = special;
}

int memread(char *buf, uint len, char special) {
    for (int i = 0; i < len; i++)
        if (buf[i] != special)
            return -1;
    return 0;
}

void memtest(char *buf, uint len) {
    memwrite(buf, len, 'y');
    if (memread(buf, len, 'y') < 0)
        FATAL("memtest: failed at reading written space");
}

void heap_ttest1() {
    /* Malloc'd Regions Retain Elements */
    void *p = ymalloc(1000);
    void *q = ymalloc(1000);
    memwrite(p, 1000, 'c');
    memwrite(q, 1000, 'm');
    if (memread(q, 1000, 'm') < 0 || memread(p, 1000, 'c') < 0)
        FATAL("heap_ttest1 failure: malloc'd memory overwritten");
    yfree(p);
    yfree(q);
}

void heap_ttest2() {
    /* Head of Free List too small for malloc'd region */
    void *q, *p = ymalloc(20);
    yfree(p);
    q = ymalloc(200);
    p = ymalloc(15);
    memwrite(p, 15, 'v');
    memwrite(q, 200, 'z');
    if (memread(q, 200, 'z') < 0 || memread(p, 15, 'v') < 0)
        FATAL("heap_ttest2: failed at write-to-read heap test");
    yfree(q);
    yfree(p);
}

void heap_ttest3() {
    /* Adjust BRK */
    void *p = ymalloc(40000);
    void *q = ymalloc(80000);

    memwrite(p, 40000, 'b');
    memwrite(q, 80000, 'w');
    if (memread(p, 40000, 'b') < 0 || memread(q, 80000, 'w') < 0)
        FATAL("heap_ttest3: brk not adjusted properly for free segments");
    yfree(p);
    yfree(q);

    p = ymalloc(20000);
    q = ymalloc(60000);
    memwrite(p, 20000, 'q');
    memwrite(q, 60000, 'r');
    if (memread(p, 20000, 'q') < 0 || memread(q, 60000, 'r') < 0)
        FATAL("heap_ttest3: cannot reuse segments after brk adjusted");
    yfree(p);
    yfree(q);
}

void heap_ttest4() {
    /* Several Small Regions */
    void *r, *s, *t, *m = ymalloc(4);
    t = ymalloc(10);
    s = ymalloc(14);
    r = ymalloc(20);
    yfree(s);
    yfree(t);
    yfree(m);
    s = ymalloc(11);
    t = ymalloc(5);
    m = ymalloc(30);

    memwrite(r, 20, 'r');
    memwrite(s, 11, 's');
    memwrite(t, 5,  't');
    memwrite(m, 30, 'm');
    if (memread(r, 20, 'r') < 0 || memread(s, 11, 's') < 0 || memread(t, 5, 't') < 0 || memread(m, 30, 'm') < 0)
        FATAL("heap_ttest4: internal fragmentation of tiny segments overlap");
    yfree(r);
    yfree(m);
    yfree(s);
    yfree(t);
}

void heap_ttest5() {
    /* Cannot use any fragmented segments */
    void *p, *q, *r, *s, *t;
    p = ymalloc(20);
    q = ymalloc(40);
    r = ymalloc(60);
    s = ymalloc(50);
    t = ymalloc(30);
    yfree(q);
    yfree(s);
    yfree(t);
    yfree(p);
    s = ymalloc(59);

    memwrite(r, 60, 'r');
    memwrite(s, 59, 's');
    if (memread(r, 60, 'r') < 0 || memread(s, 59, 's') < 0)
        FATAL("heap_ttest5: cannot search deeply into fragmented free list");
    yfree(r);
    yfree(s);
}

void heap_ttest6() {
    /* Slightly Increasing Malloc'd Region */
    for (int i = 1; i < 1000; i++) {
        void *m = ymalloc(i);
        memwrite(m, i, (char)(i % sizeof(char)));
        if (memread(m, i, (char)(i % sizeof(char))) < 0)
            FATAL("heap_ttest6: slightly increasing memtest fail on malloc'd size %d", i);
        yfree(m);
    }
}

void heap_ttest7() {
    /* Slightly Decreasing Malloc'd Region */
    for (int i = 10000; i > 1000; i--) {
        void *m = ymalloc(i);
        memwrite(m, i, (char)(i % sizeof(char)));
        if (memread(m, i, (char)(i % sizeof(char))) < 0)
            FATAL("heap_ttest7: slightly decreasing memtest fail on malloc'd size %d", i);
        yfree(m);
    }
}

void heap_ttest8() {
    /* Eviction Test */
    void *e = ymalloc(10000000);
    memwrite(e, 10000000, 'e');
    if (memread(e, 10000000, 'e') < 0)
        FATAL("heap_ttest8: cannot malloc extremely large region");
    yfree(e);
}

struct ptrs {
    uint size;
    void *loc;
    char special;
};

void heap_ttestR() {
    #define MALLOC_MAXSIZE PAGE_SIZE
    struct ptrs ptrs[RAND_NTEST];
    struct ptrs pick[RAND_NTEST / 10];

    for (int i = 0; i < RAND_NTEST; i++) {
        uint size = rand() % MALLOC_MAXSIZE;
        ptrs[i].loc = ymalloc(size);
        ptrs[i].size = size;
        ptrs[i].special = (char)i;
    }

    for (int i = 0; i < RAND_NTEST; i += (RAND_NTEST / 10))
        pick[i / 10] = ptrs[i];
    
    for (int i = 0; i < RAND_NTEST / 10; i++)
        memwrite(pick[i].loc, pick[i].size, pick[i].special);
    for (int i = 0; i < RAND_NTEST / 10; i++)
        if (memread(pick[i].loc, pick[i].size, pick[i].special))
            FATAL("heap_ttestR: failed on random pointer %d of size %d", i, pick[i].size);

    for (int i = 0; i < RAND_NTEST; i++)
        yfree(ptrs[i].loc);
}

void test_data(char *diff) {
    CRITICAL("Data Test Start");
    
    if (!strcmp(diff, DIFF_EASY))
        memtest(data, 100);
    else if (!strcmp(diff, DIFF_MED))
        memtest(data, 10000);
    else if (!strcmp(diff, DIFF_HARD))
        memtest(data, 100000);
    else if (!strcmp(diff, DIFF_IMPOSSIBLE))
        memtest(data, LARGE_NUMBER);
    else if (!strcmp(diff, DIFF_RANDOM))
        for (int i = 0; i < RAND_NTEST; i++)
            memtest(data + (rand() % LARGE_NUMBER), 1);
    else
        INFO("%s does not match any difficulty", diff);
    
    CRITICAL("Data Test End");
}

void test_heap(char *diff) {
    CRITICAL("Heap Test Start");

    if (!strcmp(diff, DIFF_EASY))
        heap_ttest1();
    else if (!strcmp(diff, DIFF_MED))
        heap_ttest2();
    else if (!strcmp(diff, DIFF_HARD)) {
        heap_ttest1();
        heap_ttest2();
        heap_ttest3();
        heap_ttest4();
        heap_ttest5();
        heap_ttest6();
        heap_ttest7();
    }
    else if (!strcmp(diff, DIFF_IMPOSSIBLE))
        heap_ttest8();
    else if (!strcmp(diff, DIFF_RANDOM))
        heap_ttestR();
    else
        INFO("%s does not match any difficulty", diff);

    CRITICAL("Heap Test End");
}

void test_stack(char *diff) {
    CRITICAL("Stack Test Start");

    if (!strcmp(diff, DIFF_EASY))
        sum(100);
    else if (!strcmp(diff, DIFF_MED))
        sum(1000);
    else if (!strcmp(diff, DIFF_HARD))
        sum(20000);
    else if (!strcmp(diff, DIFF_IMPOSSIBLE))
        sum(100000);
    else if (!strcmp(diff, DIFF_RANDOM)) {
        uint sp;
        asm("mv %0, sp":"=r"(sp));
        for (int i = 0; i < RAND_NTEST; i++) 
            memtest((void*)sp - PAGE_SIZE - (rand() % LARGE_NUMBER), 1);
    }
    else
        INFO("%s does not match any difficulty", diff);

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
        test_heap(argv[2]);
    if (!strcmp(argv[1], "stack"))
        test_stack(argv[2]);
}
