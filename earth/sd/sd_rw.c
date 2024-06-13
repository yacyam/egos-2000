/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: read and write SD card blocks
 */

#include "sd.h"
#include "disk.h"

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
    while (recv_data_byte() != 0xFF);

    /* Send write request with cmd24 */
    char *arg = (void*)&offset;
    char reply, cmd24[] = {0x58, arg[3], arg[2], arg[1], arg[0], 0xFF};
    if (reply = sd_exec_cmd(cmd24))
        FATAL("SD card replies cmd24 with status %.2x", reply);

    /* Send data packet: token + block + dummy 2-byte checksum */
    send_data_byte(0xFE);
    for (uint i = 0; i < BLOCK_SIZE; i++) send_data_byte(src[i]);
    send_data_byte(0xFF);
    send_data_byte(0xFF);

    /* Wait for SD card ack of data packet */
    while ((reply = recv_data_byte()) == 0xFF);
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
