/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: helper functions for communication with the SD card
 */

#include "sd.h"

int send_byte(char byte) {
    int is_full = REGW(SPI_BASE, SPI_TXDATA) & (1 << 31);
    if (is_full) return -1;

    if (earth->platform == ARTY)
        REGB(SPI_BASE, SPI_TXDATA) = byte;
    else /* QEMU */
        REGW(SPI_BASE, SPI_TXDATA) = byte;

    return 0;
}

int recv_byte(char *dst) {
    uint rxdata = REGW(SPI_BASE, SPI_RXDATA);

    int is_empty = rxdata & (1 << 31);
    if (is_empty) return -1;

    *dst = (char)rxdata & 0xFF;
    return 0;
}

void sd_update_read(struct sd *sd, char reply) {
    switch (sd->rdstate) {
    case SD_RD_WAIT_RESPONSE:
        if (reply == 0) sd->rdstate = SD_RD_WAIT_START;
        else FATAL("SD card replies cmd17 with status 0x%.2x", reply);
        break;
    case SD_RD_WAIT_START:
        if (reply == START_TOKEN) sd->rdstate = SD_RD_READ_BLOCK;
        break;
    }
}

void sd_update_write(struct sd *sd, char reply) {
    switch (sd->wrstate) {
    case SD_WR_WAIT_RESPONSE_1:
        if (reply == 0) sd->wrstate = SD_WR_WRITE_BLOCK;
        else FATAL("SD card replies cmd24 with status 0x%.2x", reply);
        break;
    case SD_WR_WAIT_RESPONSE_2:
        if (reply == 0x05) sd->wrstate = SD_WR_READY;
        else FATAL("SD card write ack with status 0x%.2x", reply);
        break;
    }
}

void sd_update_waiting(struct sd *sd, char reply) {
    if (reply == DUMMY_BYTE) return; /* Busy Waiting Response, Do not update state */

    if (sd->exec == SD_CMD_READ)
        sd_update_read(sd, reply);
    if (sd->exec == SD_CMD_WRITE)
        sd_update_write(sd, reply);
}

/* * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * */
/* * * * * * * * * * * * * * * * * */

void busy_send_byte(char byte) {
    while(send_byte(byte) < 0);
}

char busy_recv_byte() { 
    char byte;
    while (recv_byte(&byte) < 0);
    return byte;
}

char busy_exch_byte(char byte) {
    busy_send_byte(byte);
    return busy_recv_byte();
}

char sd_exec_cmd(char* cmd) {
    for (uint i = 0; i < 6; i++) busy_send_byte(cmd[i]);

    for (uint reply, i = 0; i < 8000; i++)
        if ((reply = busy_exch_byte(0xFF)) != 0xFF) return reply;

    FATAL("SD card not responding cmd%d", cmd[0] ^ 0x40);
}

char sd_exec_acmd(char* cmd) {
    char cmd55[] = {0x77, 0x00, 0x00, 0x00, 0x00, 0xFF};
    while (busy_exch_byte(0xFF) != 0xFF);
    sd_exec_cmd(cmd55);

    while (busy_exch_byte(0xFF) != 0xFF);
    return sd_exec_cmd(cmd);
}

static void spi_set_clock(long baud_rate) {
    long div = (CPU_CLOCK_RATE / (2 * baud_rate)) - 1;
    REGW(SPI_BASE, SPI_SCKDIV) = (div & 0xFFF);
}

enum sd_type SD_CARD_TYPE = SD_TYPE_UNKNOWN;

void sdinit() {
    /* Configure the SPI controller */
    spi_set_clock(100000);
    REGW(SPI_BASE, SPI_CSMODE) = 1;
    REGW(SPI_BASE, SPI_CSDEF) = 0;
    REGW(SPI_BASE, SPI_FCTRL) = 0;

    /* Enable SPI Interrupts */
    REGW(SPI_BASE, SPI_IE) = 2; // Only Enable Rx Interrupts
    REGW(SPI_BASE, SPI_RXMARK) = 4; // Set Watermark for Rx to 0

    INFO("Set CS and MOSI to 1 and toggle clock.");
    uint i, rxdata;
    for (i = 0; i < 1000; i++) busy_send_byte(0xFF);
    /* Keep chip select line low */
    REGW(SPI_BASE, SPI_CSDEF) = 1;
    for (i = 0; i < 200000; i++);

    INFO("Set CS to 0 and send cmd0 to SD card.");
    char cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
    char reply = sd_exec_cmd(cmd0);
    while (reply != 0x01) reply = busy_exch_byte(0xFF);
    while (busy_exch_byte(0xFF) != 0xFF);

    INFO("Set SPI clock frequency to %ldHz", CPU_CLOCK_RATE / 4);
    spi_set_clock(CPU_CLOCK_RATE / 4);

    INFO("Check SD card type and voltage with cmd8");
    char cmd8[] = {0x48, 0x00, 0x00, 0x01, 0xAA, 0x87};
    reply = sd_exec_cmd(cmd8);

    if (reply & 0x04) {
        /* Illegal command */
        SD_CARD_TYPE = SD_TYPE_SD1;
    } else {
        /* Only need the last byte of r7 response */
        uint payload;
        for (uint i = 0; i < 4; i++)
            ((char*)&payload)[3 - i] = busy_exch_byte(0xFF);
        INFO("SD card replies cmd8 with status 0x%.2x and payload 0x%.8x", reply, payload);

        if ((payload & 0xFFF) != 0x1AA) FATAL("Fail to check SD card type");
        SD_CARD_TYPE = SD_TYPE_SD2;
    }
    while (busy_exch_byte(0xFF) != 0xFF);

    char acmd41[] = {0x69, (SD_CARD_TYPE == SD_TYPE_SD2)? 0x40 : 0x00, 0x00, 0x00, 0x00, 0xFF};
    while (sd_exec_acmd(acmd41));
    while (busy_exch_byte(0xFF) != 0xFF);

    INFO("Set block size to 512 bytes with cmd16");
    char cmd16[] = {0x50, 0x00, 0x00, 0x02, 0x00, 0xFF};
    reply = sd_exec_cmd(cmd16);
    while (busy_exch_byte(0xFF) != 0xFF);

    if (SD_CARD_TYPE == SD_TYPE_SD2) {
        INFO("Check SD card capacity with cmd58");

        char reply, payload[4], cmd58[] = {0x7A, 0x00, 0x00, 0x00, 0x00, 0xFF};
        sd_exec_cmd(cmd58);
        for (uint i = 0; i < 4; i++) payload[3 - i] = busy_exch_byte(0xFF);

        if ((payload[3] & 0xC0) == 0xC0) SD_CARD_TYPE = SD_TYPE_SDHC;
        INFO("SD card replies cmd58 with payload 0x%.8x", *(int*)payload);

        while (busy_exch_byte(0xFF) != 0xFF);
    }
}
