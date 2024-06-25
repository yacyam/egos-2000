#pragma once

#include "egos.h"

enum sd_cmd {
      SD_CMD_READ,
      SD_CMD_WRITE
};

enum sd_rdstate {
      SD_RD_READY,
      SD_RD_WAIT_RESPONSE,
      SD_RD_WAIT_START,
      SD_RD_READ_BLOCK
};

enum sd_wrstate {
      SD_WR_READY,
      SD_WR_WAIT_RESPONSE_1,
      SD_WR_WRITE_BLOCK,
      SD_WR_WAIT_RESPONSE_2
};

struct sd {
    enum sd_cmd exec;
    enum sd_rdstate rdstate; /* Read CMD State */
    enum sd_wrstate wrstate; /* Write CMD State */
    uint num_read, num_written;
};

int send_byte(char);
int recv_byte(char *);

void sd_update_waiting(struct sd *, char);
int sd_start_cmd(uint, enum sd_cmd);
int sd_spi_intr(char *);


/* * * * * * * * * * * * */

enum sd_type {
      SD_TYPE_SD1,
      SD_TYPE_SD2,
      SD_TYPE_SDHC,
      SD_TYPE_UNKNOWN
};
extern enum sd_type SD_CARD_TYPE;

char busy_recv_byte();
char busy_exch_byte(char);
void busy_send_byte(char);

char sd_exec_cmd(char*);
char sd_exec_acmd(char*);

void sdinit();
int sdread(uint offset, uint nblock, char* dst);
int sdwrite(uint offset, uint nblock, char* src);

#define CMD_LEN      6
#define DUMMY_BYTE   0xFF
#define START_TOKEN  0xFE

/* definitions for controlling SPI1 in FE310
 * see chapter19 of the SiFive FE310-G002 Manual
 */

#define SPI_SCKDIV   0UL
#define SPI_SCKMODE  4UL
#define SPI_CSID     16UL
#define SPI_CSDEF    20UL
#define SPI_CSMODE   24UL
#define SPI_FMT      64UL
#define SPI_TXDATA   72UL
#define SPI_RXDATA   76UL
#define SPI_RXMARK   84UL
#define SPI_FCTRL    96UL
#define SPI_IE       112UL
#define SPI_IP       116UL

#define SPI_QUEUE_SIZE 8