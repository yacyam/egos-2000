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
    uint idx, frame_id;
    /* Randomly select a cache slot pid is not using to evict */
    do {
        idx = rand() % ARTY_CACHED_NFRAMES;
    } while (cache_slots[idx].pid == pid || cache_slots[idx].pinned);
    
    frame_id = cache_slots[idx].frame_id;
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
