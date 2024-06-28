/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: read and write SD card blocks
 */

#include "sd.h"
#include "disk.h"

struct sd sd_card;

void sd_send_cmd(uint block_no, enum sd_cmd rw) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) block_no = block_no * BLOCK_SIZE;

    char *arg = (char *)&block_no;
    char cmd[] = {
        (rw == SD_CMD_READ) ? 0x51 : 0x58, 
        arg[3], arg[2], arg[1], arg[0], 
        DUMMY_BYTE
    };

    for (int i = 0; i < CMD_LEN; i++) send_byte(cmd[i]); 
}

int sd_start_cmd(uint block_no, enum sd_cmd cmd) {
    /* Can only send 1 CMD at a time */
    if (sd_card.rdstate != SD_RD_READY || sd_card.wrstate != SD_WR_READY) return -1;
    
    sd_card.exec = cmd;
    sd_send_cmd(block_no, cmd);

    sd_card.num_read = 0;
    sd_card.num_written = CMD_LEN;

    /* Update CMD-Specific State, Interrupts will drive executing SD Card CMD */
    if (cmd == SD_CMD_READ)  sd_card.rdstate = SD_RD_WAIT_RESPONSE;
    if (cmd == SD_CMD_WRITE) sd_card.wrstate = SD_WR_WAIT_RESPONSE_1;
    return 0;
}

void sd_read_block(char *dst) {
    int i = 0;

    while (sd_card.num_read < sd_card.num_written) {
        dst[i++] = busy_recv_byte();
        sd_card.num_read++;
    }

    for (; i < BLOCK_SIZE; i++) dst[i] = busy_exch_byte(DUMMY_BYTE);
    
    /* Two Byte Checksum */
    busy_exch_byte(DUMMY_BYTE);
    busy_exch_byte(DUMMY_BYTE);
}

void sd_write_block(char *src) {
    while (sd_card.num_read < sd_card.num_written) {
        busy_recv_byte();
        sd_card.num_read++;
    }

    /* Send Start Token + Block */
    busy_exch_byte(START_TOKEN);
    for (int i = 0; i < BLOCK_SIZE; i++) busy_exch_byte(src[i]);
}

int sd_spi_intr(char *dst) {
    char reply;
    if (recv_byte(&reply) < 0) return -1; // Spurious Interrupt
    if (sd_card.rdstate == SD_RD_READY && sd_card.wrstate == SD_WR_READY) return -1; // No CMD is running

    do {
        sd_card.num_read++;
        sd_update_waiting(&sd_card, reply);

        if (sd_card.rdstate == SD_RD_READ_BLOCK) { 
            sd_read_block(dst);
            while (busy_exch_byte(DUMMY_BYTE) != DUMMY_BYTE); // Wait until SD Card is no longer busy
            sd_card.rdstate = SD_RD_READY;
            return 0;
        }
    } while (recv_byte(&reply) == 0);

    /* Drive SD Clock to induce another Rx Interrupt with Non-Busy Response */
    while ( 
        sd_card.num_written < sd_card.num_read + SPI_QUEUE_SIZE &&
        send_byte(DUMMY_BYTE) == 0 ) { 
        sd_card.num_written++;
    }
    if (sd_card.num_read == 8000) FATAL("sd_spi_intr: sd card not responding");
    return -1;
}

static void single_read(uint offset, char* dst) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) offset = offset * BLOCK_SIZE;
    
    /* Send read request with cmd17 */
    char *arg = (void*)&offset;
    char reply, cmd17[] = {0x51, arg[3], arg[2], arg[1], arg[0], 0xFF};

    while (recv_byte(&reply) == 0);   // Read Out Remaining Chars
    while (busy_exch_byte(0xFF) != 0xFF); // Wait Until SD Card is not Busy

    for (int i = 0; i < 6; i++) busy_exch_byte(cmd17[i]); // Send Command, Read Busy Response

    while ((reply = busy_exch_byte(0xFF)) == 0xFF); // Cmd Reply

    if (reply != 0) FATAL("SD card replies cmd17 with status 0x%.2x", reply);
    
    while (busy_exch_byte(0xFF) != 0xFE); // Start Token

    for (int i = 0; i < BLOCK_SIZE; i++) dst[i] = busy_exch_byte(0xFF);
    
    // Two Byte Checksum
    busy_exch_byte(0xFF);
    busy_exch_byte(0xFF);

    while (busy_exch_byte(0xFF) != 0xFF); // Wait Until SD Card is not Busy
}

static void single_write(uint offset, char* src) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) offset = offset * BLOCK_SIZE;

    /* Send write request with cmd24 */
    char *arg = (void*)&offset;
    char reply, cmd24[] = {0x58, arg[3], arg[2], arg[1], arg[0], 0xFF};

    /* Wait until SD card is not busy */
    while (recv_byte(&reply) == 0);   // Read Out Remaining Chars
    while (busy_exch_byte(0xFF) != 0xFF);

    for (int i = 0; i < 6; i++) busy_exch_byte(cmd24[i]);// Send Command

    while ((reply = busy_exch_byte(0xFF)) == 0xFF); // Cmd Reply

    if (reply) 
        FATAL("SD card replies cmd24 with status %.2x", reply);

    busy_exch_byte(0xFF); /* At Least 1 Byte buffer before sending Block */

    /* Send data packet: token + block + dummy 2-byte checksum */
    busy_exch_byte(0xFE);
    for (uint i = 0; i < BLOCK_SIZE; i++) busy_exch_byte(src[i]);

    /* Wait for SD card ack of data packet */
    while ((reply = busy_exch_byte(0xFF)) == 0xFF);
    if ((reply & 0x1F) != 0x05)
       FATAL("SD card write ack with status 0x%.2x", reply);

    while (busy_exch_byte(0xFF) != 0xFF); // Wait Until SD Card is not Busy
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
