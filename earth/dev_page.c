/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a 1MB (256*4KB) paging device
 * for QEMU, 256 physical frames start at address FRAME_CACHE_START
 * for Arty, 28 physical frames are cached at address FRAME_CACHE_START
 * and 256 frames (1MB) start at the beginning of the microSD card
 */

#include "egos.h"
#include "disk.h"
#include <stdlib.h>
#include <string.h>
#define ARTY_CACHED_NFRAMES 28

struct frame_cache {
    int frame_id; /* Physical Frame Associated with Entry */
    int pid;      /* PID which owns Frame */
    int pinned;   /* Whether Frame can be evicted to Disk */
} cache_slots[ARTY_CACHED_NFRAMES];
char *pages_start = (void*)FRAME_CACHE_START;

static uint cache_eviction(int pid) {
    uint idx, idx1, idx2, idx3, frame_id;
    /* Randomly select a cache slot pid is not using to evict */
    //for (int i = 0; i < ARTY_CACHED_NFRAMES; i++)
    //    if (cache_slots[i].pid != pid) idx1 = i;

    do {
        idx2 = rand() % ARTY_CACHED_NFRAMES;
    } while (cache_slots[idx2].pid == pid || cache_slots[idx2].pinned);
    /*
    for (int i = 0; i < ARTY_CACHED_NFRAMES; i++)
        if (cache_slots[i].pid == 1 && pid != 1) { 
            idx3 = i;
            break;
        } else if (cache_slots[i].pid != 1) idx3 = i;
    */
    // CRITICAL("\r\n cache_eviction::: evicter: %d,\r\n scan: (i: %d, pid: %d),\r\n rand: (i: %d, pid: %d),\r\n pick: (i: %d, pid: %d)", 
    //     pid, idx1, cache_slots[idx1].pid, idx2, cache_slots[idx2].pid, idx3, cache_slots[idx3].pid);
    idx = idx2;
    
    frame_id = cache_slots[idx].frame_id;

    // if (cache_slots[idx2].pid == 3) CRITICAL("WB to Disk, frame_id: %d, byte: %x", frame_id, *(pages_start + (PAGE_SIZE * idx)));
    earth->disk_write_kernel(frame_id * NBLOCKS_PER_PAGE, NBLOCKS_PER_PAGE, pages_start + (PAGE_SIZE * idx));
    return idx;
}

static void paging_update_pin(int pid, int frame_id, int choice) {
    for (int i = 0; i < ARTY_CACHED_NFRAMES; i++)
        if (cache_slots[i].pid == pid && cache_slots[i].frame_id == frame_id) {
            cache_slots[i].pinned = choice;
            return;
        }
}

void paging_pin(int pid, int frame_id) { paging_update_pin(pid, frame_id, 1); }

void paging_unpin(int pid, int frame_id) { paging_update_pin(pid, frame_id, 0); }

void paging_init() { memset(cache_slots, 0xFF, sizeof(cache_slots)); }

int paging_invalidate_cache(uint frame_id) {
    for (uint j = 0; j < ARTY_CACHED_NFRAMES; j++)
        if (cache_slots[j].frame_id == frame_id) cache_slots[j].frame_id = -1;
}

int paging_write(int pid, uint frame_id, uint page_no) {
    char* src = (void*)(page_no << 12);
    /*
    if (earth->platform != ARTY) {
        memcpy(pages_start + frame_id * PAGE_SIZE, src, PAGE_SIZE);
        return 0;
    }
    */

    for (uint i = 0; i < ARTY_CACHED_NFRAMES; i++)
        if (cache_slots[i].frame_id == frame_id && cache_slots[i].pid == pid) {
            // INFO("paging_write::: frame_id: %d, cache_pid: %d, curr_pid: %d", cache_slots[i].frame_id, cache_slots[i].pid, pid);
            memcpy(pages_start + PAGE_SIZE * i, src, PAGE_SIZE);
            return 0;
        }

    uint free_idx = cache_eviction(pid);
    cache_slots[free_idx].frame_id = frame_id;
    cache_slots[free_idx].pid = pid;
    cache_slots[free_idx].pinned = 0;

    memcpy(pages_start + PAGE_SIZE * free_idx, src, PAGE_SIZE);
}

char* paging_read(int pid, uint frame_id, int alloc_only) {
    /*
    if (earth->platform != ARTY) return pages_start + frame_id * PAGE_SIZE;
    */
    int free_idx = -1;
    for (uint i = 0; i < ARTY_CACHED_NFRAMES; i++) {
        if (cache_slots[i].frame_id == -1 && free_idx == -1) free_idx = i;
        if (cache_slots[i].frame_id == frame_id) return pages_start + PAGE_SIZE * i;
    }

    if (free_idx == -1) free_idx = cache_eviction(pid);
    cache_slots[free_idx].frame_id = frame_id;
    cache_slots[free_idx].pid = pid;
    cache_slots[free_idx].pinned = 0;

    // INFO("paging_read::: from: %d, frame: %d", pid, frame_id);
    if (!alloc_only)
        earth->disk_read_kernel(frame_id * NBLOCKS_PER_PAGE, NBLOCKS_PER_PAGE, pages_start + PAGE_SIZE * free_idx);

    return pages_start + PAGE_SIZE * free_idx;
}

void page_test_1() {
    SUCCESS("page_test_1:::START");
    const int PID_1 = 1, PID_2 = 2;
    const int __pid_1_frame_id_1 = 10, __pid_1_frame_id_2 = 120;
    const int __pid_2_frame_id_1 = 8, __pid_2_frame_id_2 = 9;

    CRITICAL("page_test::: write vpa, wb to cache, evict to disk, bring back to cache, read vpa");

    char *vpa1 = (char *)APPS_ENTRY;
    char *vpa2 = (char *)APPS_ENTRY + 0x1000;
    vpa1[0] = 'y';
    vpa2[0] = 'a';

    paging_read(PID_1, __pid_1_frame_id_1, 1); /* Allocate Cache Entry with frame_id */
    paging_read(PID_1, __pid_1_frame_id_2, 1); /* Allocate Cache Entry with frame_id */
    paging_write(PID_1, __pid_1_frame_id_1, (int)vpa1 >> 12); /* wb to cache */
    paging_write(PID_1, __pid_1_frame_id_2, (int)vpa2 >> 12);

    CRITICAL("page_test::: initial wb to cache, cache byte 1: %c, cache byte 2: %c", *((char *)pages_start), *((char *)pages_start + PAGE_SIZE));

    paging_read(PID_2, __pid_2_frame_id_1, 1);
    paging_read(PID_2, __pid_2_frame_id_2, 1); /* Evict PID_1's entry */
    char *ptr1 = paging_read(PID_1, __pid_1_frame_id_1, 0);
    char *ptr2 = paging_read(PID_1, __pid_1_frame_id_2, 0);

    if (ptr1[0] != 'y' || ptr2[0] != 'a')
        FATAL("page_test_1::: evicted and brought back; frame 1 (initial: %c, ptr1: %c), frame 2 (initial %c, ptr2: %c)", 'y', ptr1[0], 'a', ptr2[0]);

    CRITICAL("page_test::: evicted and brought back, cache byte 1: %c, cache byte 2: %c", ptr1[0], ptr2[0]);


    SUCCESS("page_test_1:::DONE");
}

void page_test_2() {
    SUCCESS("page_test_2:::START");

    const int PID_1 = 1, PID_2 = 2;
    const int __pid_1_frame_id_1 = 1, __pid_1_frame_id_2 = 2;
    const int __pid_2_frame_id_1 = 3, __pid_2_frame_id_2 = 4;

    CRITICAL("page_test::: same ast test_1, but PID_2 wb to cache before PID_1 is brought back");

    char *vpa1 = (char *)APPS_ENTRY;
    char *vpa2 = (char *)APPS_ENTRY + 0x1000;
    vpa1[0] = 'y';
    vpa2[0] = 'a';

    paging_read(PID_1, __pid_1_frame_id_1, 1); /* Allocate Cache Entry with frame_id */
    paging_read(PID_1, __pid_1_frame_id_2, 1); /* Allocate Cache Entry with frame_id */
    paging_write(PID_1, __pid_1_frame_id_1, (int)vpa1 >> 12); /* wb to cache */
    paging_write(PID_1, __pid_1_frame_id_2, (int)vpa2 >> 12);

    CRITICAL("page_test::: PID_1 initial wb to cache, cache byte 1: %c, cache byte 2: %c", *((char *)pages_start), *((char *)pages_start + PAGE_SIZE));

    paging_read(PID_2, __pid_2_frame_id_1, 1);
    paging_read(PID_2, __pid_2_frame_id_2, 1); /* Evict PID_1's entry */
    
    vpa1[0] = 'a';
    vpa2[0] = 'b';

    paging_write(PID_2, __pid_2_frame_id_1, (int)vpa1 >> 12); /* wb to cache */
    paging_write(PID_2, __pid_2_frame_id_2, (int)vpa2 >> 12);

    CRITICAL("page_test::: PID_2 initial wb to cache, cache byte 1: %c, cache byte 2: %c", *((char *)pages_start), *((char *)pages_start + PAGE_SIZE));

    char *ptr1 = paging_read(PID_1, __pid_1_frame_id_1, 0); /* Bring back PID_1's entries */
    char *ptr2 = paging_read(PID_1, __pid_1_frame_id_2, 0);

    CRITICAL("page_test::: PID_1 evicted and brought back, cache byte 1: %c, cache byte 2: %c", ptr1[0], ptr2[0]);

    ptr1 = paging_read(PID_2, __pid_2_frame_id_1, 0);
    ptr2 = paging_read(PID_2, __pid_2_frame_id_2, 0);

    CRITICAL("page_test::: PID_2 evicted and brought back, cache byte 1: %c, cache byte 2: %c", ptr1[0], ptr2[0]);

    SUCCESS("page_test_2:::DONE");
}

void page_test_3() {
    SUCCESS("page_test_3:::START");

    CRITICAL("Write to larger locations in cache and evict them");

    char volatile *src = pages_start;

    for (int i = 0; i < 28; i++) {
        src = pages_start + (PAGE_SIZE * i);

        for (int j = 0; j < NBLOCKS_PER_PAGE; j++) {
            src[BLOCK_SIZE * j] = j;
        }

        for (int j = 0; j < NBLOCKS_PER_PAGE; j++) {
            if (src[BLOCK_SIZE * j] != j) FATAL("NEQ");
        }

        earth->disk_write_kernel(i * NBLOCKS_PER_PAGE, NBLOCKS_PER_PAGE, (char *)src);
        earth->disk_read_kernel(i * NBLOCKS_PER_PAGE, NBLOCKS_PER_PAGE, (char *)src);

        for (int j = 0; j < NBLOCKS_PER_PAGE; j++) {
            if (src[BLOCK_SIZE * j] != j) FATAL("NEQ");
        }

        src += PAGE_SIZE;
    }

    SUCCESS("page_test_3:::DONE");
}

void page_test_4() {
    SUCCESS("page_test_4:::START");

    CRITICAL("Multiple writes to disk without immediate read");

    char *src = pages_start;

    for (int i = 0; i < 28; i++) {
        for (int j = 0; j < NBLOCKS_PER_PAGE; j++) {
            src[BLOCK_SIZE * j] = j;
        }

        earth->disk_write_kernel(i * NBLOCKS_PER_PAGE, NBLOCKS_PER_PAGE, src);
        src += PAGE_SIZE;
    }

    src = pages_start;

    for (int i = 0; i < 28; i++) {
        earth->disk_read_kernel(i * NBLOCKS_PER_PAGE, NBLOCKS_PER_PAGE, src);

        for (int j = 0; j < NBLOCKS_PER_PAGE; j++) {
            if (src[BLOCK_SIZE * j] != j) FATAL("NEQ");
        }

        src += PAGE_SIZE;
    }

    SUCCESS("page_test_4:::DONE");
}

void page_test() {
    return;
    printf("\n\n\n\n");
    page_test_1();
    paging_init();
    page_test_2();
    paging_init();
    page_test_3();
    paging_init();
    page_test_4();
    while (2);
}
