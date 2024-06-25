/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a simple disk device driver
 */

#include "egos.h"
#include "disk.h"
#include "sd/sd.h"
#include "bus_gpio.c"
#include <string.h>

enum disk_type {
      SD_CARD,
      FLASH_ROM
};
static enum disk_type type;

struct disk_read_cmd {
    enum {
        DISK_IDLE,    /* Waiting for Command */
        DISK_RUNNING, /* Executing Command */
        DISK_FINISHED /* Waiting for User to Read Block */
    } state;
    block_no block_no;
    block_t data;
};

static struct disk_read_cmd read_cmd;

int disk_intr() {
    /* SD Card Read Out Block into Buffer */
    if (sd_spi_intr(read_cmd.data.bytes) == 0 && read_cmd.state == DISK_RUNNING)
        read_cmd.state = DISK_FINISHED;
    
    return 0;
}

void disk_read_kernel(uint block_no, uint nblocks, char* dst) {
    if (type == SD_CARD) {
        sdread(block_no, nblocks, dst);
    } else {
        char* src = (char*)0x20800000 + block_no * BLOCK_SIZE;
        memcpy(dst, src, nblocks * BLOCK_SIZE);
    }
}

int disk_read(uint block_no, uint nblocks, char* dst) {
    if (type == FLASH_ROM) {
        char* src = (char*)0x20800000 + block_no * BLOCK_SIZE;
        memcpy(dst, src, nblocks * BLOCK_SIZE);
        return 0;
    } 
    CRITICAL("TRYING TO SEND: %x, STATE: %d", block_no, read_cmd.state);
    if (read_cmd.state == DISK_IDLE && sd_start_cmd(block_no, SD_CMD_READ) == 0) {
        SUCCESS("SENT");
        read_cmd.block_no = block_no;
        read_cmd.state = DISK_RUNNING;
    }
    
    if (read_cmd.state == DISK_FINISHED && block_no == read_cmd.block_no) {
        SUCCESS("READ");
        memcpy(dst, read_cmd.data.bytes, BLOCK_SIZE);
        read_cmd.state = DISK_IDLE;
        return 0;
    }

    return -1;
}

int disk_write(uint block_no, uint nblocks, char* src) {
    if (type == FLASH_ROM)
        FATAL("disk_write: Writing to the read-only ROM");

    sdwrite(block_no, nblocks, src);
    return 0;
}

void disk_init() {
    earth->disk_read = disk_read;
    earth->disk_write = disk_write;
    earth->disk_read_kernel = disk_read_kernel;

    read_cmd.state = DISK_IDLE;

    if (earth->platform == QEMU_SIFIVE) {
        /* SiFive QEMU v5 does not support SD card emulation */
        type = FLASH_ROM;
        return;
    }

    CRITICAL("Choose a disk:");
    printf("Enter 0: microSD card\r\nEnter 1: on-board ROM\r\n");

    char buf[2];
    for (buf[0] = 0; buf[0] != '0' && buf[0] != '1'; earth->tty_read(buf, 2));
    type = (buf[0] == '0')? SD_CARD : FLASH_ROM;
    INFO("%s is chosen", type == SD_CARD? "microSD" : "on-board ROM");

    if (type == SD_CARD) sdinit();
}
