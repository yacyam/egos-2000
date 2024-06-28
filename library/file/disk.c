/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: the inode_store structure for accessing the physical disk
 */

#include "egos.h"
#include "inode.h"

static int disk_getsize() { return FS_DISK_SIZE / BLOCK_SIZE; }

static int disk_setsize() { FATAL("disk: cannot set the size"); }

static int disk_read(inode_intf bs, uint ino, block_no offset, block_t *block) {
    return grass->sys_disk(GRASS_FS_START + offset, 1, block->bytes, IO_READ);
}

static int disk_write(inode_intf bs, uint ino, block_no offset, block_t *block) {
    return grass->sys_disk(GRASS_FS_START + offset, 1, block->bytes, IO_WRITE);
}

static inode_store_t disk;

inode_intf fs_disk_init() {
    disk.read = disk_read;
    disk.write = disk_write;
    disk.getsize = disk_getsize;
    disk.setsize = disk_setsize;

    return &disk;
}
