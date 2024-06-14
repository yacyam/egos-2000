#pragma once

#include "egos.h"

enum sd_state {
      SD_READY,
      SD_WAIT_RESPONSE,
      SD_WAIT_START,
      SD_READ_BLOCK,
      SD_BUSY
};

int send_byte(char);
int recv_byte(char *);

int sd_send_cmd(uint);
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
char busy_send_byte(char);

char sd_exec_cmd(char*);
char sd_exec_acmd(char*);

void sdinit();
int sdread(uint offset, uint nblock, char* dst);
int sdwrite(uint offset, uint nblock, char* src);

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