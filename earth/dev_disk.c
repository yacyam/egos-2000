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

struct disk_cmd {
    int exec;          /* What type of Command is Executing? */
    enum {
        DISK_IDLE,     /* Waiting for Command */
        DISK_RUNNING,  /* Executing Command */
        DISK_FINISHED, /* Waiting for User to Read Block */
    } state;
    block_no block_no;
    block_t data;
};
static struct disk_cmd disk_cmd;

int disk_intr() {
    /* Was Disk Command completed by SD Card */
    if (sd_spi_intr(disk_cmd.data.bytes) == 0 && disk_cmd.state == DISK_RUNNING) {
        disk_cmd.state = (disk_cmd.exec == IO_READ) ? DISK_FINISHED : DISK_IDLE;
        return 0;
    }
    
    return -1;
}

void kernel_disk_flush() {
    if (disk_cmd.state == DISK_RUNNING)
        while (disk_intr() != 0); // Once interrupt succeeds, entire block is read/written
}

void kernel_disk_read(uint block_no, uint nblocks, char* dst) {
    if (type == SD_CARD) {
        kernel_disk_flush();
        kernel_sd_read(block_no, nblocks, dst);
    } else {
        char* src = (char*)0x20800000 + block_no * BLOCK_SIZE;
        memcpy(dst, src, nblocks * BLOCK_SIZE);
    }
}

void kernel_disk_write(uint block_no, uint nblocks, char* src) {
    if (type == FLASH_ROM)
        FATAL("disk_write: Writing to the read-only ROM");

    kernel_disk_flush();
    kernel_sd_write(block_no, nblocks, src);
}

int disk_read(uint block_no, uint nblocks, char* dst) {
    if (type == FLASH_ROM) {
        char* src = (char*)0x20800000 + block_no * BLOCK_SIZE;
        memcpy(dst, src, nblocks * BLOCK_SIZE);
        return 0;
    } 

    if (disk_cmd.state == DISK_IDLE && sd_start_cmd(block_no, SD_CMD_READ) == 0) {
        disk_cmd.block_no = block_no;
        disk_cmd.state = DISK_RUNNING;
        disk_cmd.exec = IO_READ;
    }
    
    if (disk_cmd.state == DISK_FINISHED && block_no == disk_cmd.block_no) {
        memcpy(dst, disk_cmd.data.bytes, BLOCK_SIZE);
        disk_cmd.state = DISK_IDLE;
        return 0;
    }

    return -1;
}

int disk_write(uint block_no, uint nblocks, char* src) {
    if (type == FLASH_ROM)
        FATAL("disk_write: Writing to the read-only ROM");

    if (disk_cmd.state == DISK_IDLE && sd_start_cmd(block_no, SD_CMD_WRITE) == 0) {
        memcpy(disk_cmd.data.bytes, src, BLOCK_SIZE);

        disk_cmd.block_no = block_no;
        disk_cmd.state = DISK_RUNNING;
        disk_cmd.exec = IO_WRITE;
        return 0;
    }

    return -1;
}

void disk_init() {
    earth->disk_read = disk_read;
    earth->disk_write = disk_write;
    earth->kernel_disk_read = kernel_disk_read;
    earth->kernel_disk_write = kernel_disk_write;

    disk_cmd.state = DISK_IDLE;

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
