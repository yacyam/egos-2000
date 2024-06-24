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

int sd_send_cmd(uint block_no) {
    if (SD_CARD_TYPE == SD_TYPE_SD2) block_no = block_no * BLOCK_SIZE;
    if (sd_card.state != SD_READY) return -1;

    char *arg = (char *)&block_no;
    char cmd17[] = {0x51, arg[3], arg[2], arg[1], arg[0], 0xFF};

    for (int i = 0; i < 6; i++) send_byte(cmd17[i]); 

    sd_card.num_read = 0;
    sd_card.num_written = 6;
    sd_card.state = SD_WAIT_RESPONSE;
    return 0;
}

void sd_read_block(char *dst) {
    int i = 0;

    while (sd_card.num_read < sd_card.num_written) {
        dst[i++] = busy_recv_byte();
        sd_card.num_read++;
    }

    for (; i < BLOCK_SIZE; i++) dst[i] = busy_exch_byte(0xFF);
    
    /* Two Byte Checksum */
    busy_exch_byte(0xFF);
    busy_exch_byte(0xFF);
}

int sd_spi_intr(char *dst) {
    char reply;
    if (recv_byte(&reply) < 0) return -1; // Spurious Interrupt

    do {
        sd_card.num_read++;
        if (reply == 0xFF) continue; // Busy Signal

        if (sd_card.state == SD_WAIT_RESPONSE && reply == 0) {
            sd_card.state = SD_WAIT_START;
        } 
        else if (sd_card.state == SD_WAIT_START && reply == 0xFE) {
            sd_card.state = SD_READ_BLOCK;
            break;
        } 
        else {
            FATAL("SD card replies cmd17 with status 0x%.2x", reply);
        }
    } while (recv_byte(&reply) == 0);

    if (sd_card.state == SD_READ_BLOCK) {
        sd_read_block(dst);
        while (busy_exch_byte(0xFF) != 0xFF); // Wait until SD Card is no longer busy
        sd_card.state = SD_READY;
        return 0;
    } else if (sd_card.state == SD_WAIT_RESPONSE || sd_card.state == SD_WAIT_START) {
        /* Drive SD Clock to induce another Rx Interrupt with Block Data */
        while ( 
            sd_card.num_written < sd_card.num_read + SPI_QUEUE_SIZE &&
            send_byte(0xFF) == 0 ) { 
            sd_card.num_written++;
        }
        if (sd_card.num_read == 8000) FATAL("sd_spi_intr: sd card not responding");
    }
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

    CRITICAL("offset: %d", offset);

    /* Send write request with cmd24 */
    char *arg = (void*)&offset;
    char reply, cmd24[] = {0x58, arg[3], arg[2], arg[1], arg[0], 0xFF};

    /* Wait until SD card is not busy */
    while (recv_byte(&reply) == 0);   // Read Out Remaining Chars
    while (busy_exch_byte(0xFF) != 0xFF);

    for (int i = 0; i < 6; i++) { 
        CRITICAL("out, %x, %x", cmd24[i], busy_exch_byte(cmd24[i]));
    } // Send Command

    while ((reply = busy_exch_byte(0xFF)) == 0xFF); // Cmd Reply

    if (reply) FATAL("SD card replies cmd24 with status %.2x", reply);

    SUCCESS("send packet");

    /* Send data packet: token + block + dummy 2-byte checksum */
    CRITICAL("first: %x", busy_exch_byte(0xFE));
    for (uint i = 0; i < BLOCK_SIZE; i++) {
        if ((reply = busy_exch_byte(src[i])) != 0xFF) INFO("diff: %x", reply);
    }

    SUCCESS("sent");

    /* Wait for SD card ack of data packet */
    while ((reply = busy_exch_byte(0xFF)) == 0xFF);
    if ((reply & 0x1F) != 0x05)
       FATAL("SD card write ack with status 0x%.2x", reply);

    SUCCESS("past");

    while (busy_exch_byte(0xFF) != 0xFF); // Wait Until SD Card is not Busy

    CRITICAL("done");
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
