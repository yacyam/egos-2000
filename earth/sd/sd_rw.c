/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: read and write SD card blocks
 */

#include "sd.h"
#include "disk.h"

struct sd {
    enum sd_state state;
    uint num_read, num_written;
};

struct sd sd_card;

int sd_send_cmd(uint block_no) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) block_no = block_no * BLOCK_SIZE;
    CRITICAL("sd_send_cmd: block %d, pend %d", block_no, REGW(SPI_BASE, SPI_IP));
    if (sd_card.state == SD_BUSY) send_byte(0xFF);
    if (sd_card.state != SD_READY) return -1; // Let Interrupt Handler Clear Out Rx

    sd_card.num_read = 0; // Reset CMD Window

    char *arg = (char *)&block_no;
    char cmd17[] = {0x51, arg[3], arg[2], arg[1], arg[0], 0xFF};

    for (int i = 0; i < 6; i++) send_byte(cmd17[i]); 
    CRITICAL("sd_send_cmd: sent block");
    sd_card.num_written = 6;
    sd_card.state = SD_WAIT_RESPONSE;
    return 0;
}

int sd_read_block(char *dst) {
    INFO("sd_read_block: addr %x", dst);
    int i = 0;

    while (sd_card.num_read < sd_card.num_written) {
        while(recv_byte(&dst[i]) < 0);
        sd_card.num_read++;
        i++;
    }

    for (; i < BLOCK_SIZE; i++) dst[i] = busy_recv_byte();
    
    
    busy_recv_byte();
    busy_recv_byte();
    sd_card.state = SD_BUSY;
    CRITICAL("sd_read_block: finished block");
    return 0;
}

int sd_spi_intr(char *dst) {
    char reply;
    INFO("sd_spi_intr: state %d", sd_card.state);
    if (sd_card.state == SD_BUSY || sd_card.state == SD_READY) {
        while (recv_byte(&reply) == 0) sd_card.state = reply == 0xFF ? SD_READY : SD_BUSY;
        return -1;
    }
    if (recv_byte(&reply) < 0) return -1; // Nothing in Rx, Spurious Interrupt

    do {
        CRITICAL("sd_spi_intr: recv byte %x, state %d", reply, sd_card.state);
        sd_card.num_read++;
        if (reply == 0xFF) continue;

        if (sd_card.state == SD_WAIT_RESPONSE) {
            if (reply == 0) { sd_card.state = SD_WAIT_START; }
            else { FATAL("SD card replies cmd17 with status 0x%.2x", reply); }
        } 
        else if (sd_card.state == SD_WAIT_START) {
            sd_card.state = SD_READ_BLOCK;
            break;
        }
    } while (recv_byte(&reply) == 0);

    CRITICAL("sd_spi_intr: state %d, n_read %d, n_written %d", sd_card.state, sd_card.num_read, sd_card.num_written);

    if (sd_card.state == SD_READ_BLOCK)
        return sd_read_block(dst);
    else {
        while ( sd_card.num_written < sd_card.num_read + SPI_QUEUE_SIZE &&
                send_byte(0xFF) == 0 ) { 
            sd_card.num_written++;
        }
        CRITICAL("sd_spi_intr: Not Read, n_read %d, n_written %d", sd_card.num_read, sd_card.num_written);
        if (sd_card.num_written > 50) FATAL("hang");
        return -1;
    }
}

static void single_read(uint offset, char* dst) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) offset = offset * BLOCK_SIZE;
    
    /* Send read request with cmd17 */
    char *arg = (void*)&offset;
    char reply, cmd17[] = {0x51, arg[3], arg[2], arg[1], arg[0], 0xFF};

    do {
        send_byte(0xFF);
        recv_byte(&reply);
    } while (reply != 0xFF); // Wait Until SD Card is not Busy

    for (int i = 0; i < 6; i++) while (send_byte(cmd17[i]) < 0); // Send Command
    for (int i = 0; i < 6; i++) while (recv_byte(&reply) < 0); // Read Busy Response on MISO Line

    do {
        while (send_byte(0xFF) < 0); 
        while (recv_byte(&reply) < 0);
    } while (reply == 0xFF); // Cmd Reply

    if (reply != 0) FATAL("SD card replies cmd17 with status 0x%.2x", reply);
    
    do {
        while (send_byte(0xFF) < 0); 
        while (recv_byte(&reply) < 0);
    } while (reply != 0xFE); // Start Token

    for (int i = 0; i < BLOCK_SIZE; i++) {
        while (send_byte(0xFF) < 0); // Send for Block
        while (recv_byte(&dst[i]) < 0); // Receive for Byte of Block
    }

    while(send_byte(0xFF) < 0);
    while(send_byte(0xFF) < 0);
    while (recv_byte(&reply) < 0);
    while (recv_byte(&reply) < 0); // Two Byte Checksum
}

static void single_write(uint offset, char* src) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) offset = offset * BLOCK_SIZE;

    /* Wait until SD card is not busy */
    while (busy_recv_byte() != 0xFF);

    /* Send write request with cmd24 */
    char *arg = (void*)&offset;
    char reply, cmd24[] = {0x58, arg[3], arg[2], arg[1], arg[0], 0xFF};
    if (reply = sd_exec_cmd(cmd24))
        FATAL("SD card replies cmd24 with status %.2x", reply);

    /* Send data packet: token + block + dummy 2-byte checksum */
    busy_send_byte(0xFE);
    for (uint i = 0; i < BLOCK_SIZE; i++) busy_send_byte(src[i]);
    busy_send_byte(0xFF);
    busy_send_byte(0xFF);

    /* Wait for SD card ack of data packet */
    while ((reply = busy_recv_byte()) == 0xFF);
    if ((reply & 0x1F) != 0x05)
        FATAL("SD card write ack with status 0x%.2x", reply);
}

int sdread(uint offset, uint nblock, char* dst) {
    /* A better way to read multiple blocks using SD card
     * command 18 is left to students as a course project */

    for (uint i = 0; i < nblock; i++)
        single_read(offset + i, dst + BLOCK_SIZE * i);
    return 0;
}

int sdwrite(uint offset, uint nblock, char* src) {
    /* A better way to write multiple blocks using SD card
     * command 25 is left to students as a course project */

    for (uint i = 0; i < nblock; i++)
        single_write(offset + i, src + BLOCK_SIZE * i);
    return 0;
}
