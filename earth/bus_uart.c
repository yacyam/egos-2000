/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: definitions for UART in FE310
 * see chapter18 of the SiFive FE310-G002 Manual
 */

#include "egos.h"
#include "tty.h"
#include "bus_gpio.c"

void uart_init(long baud_rate) {
    REGW(UART_BASE, UART_DIV) = CPU_CLOCK_RATE / baud_rate - 1;
    REGW(UART_BASE, UART_TXCTRL) |= 1;
    REGW(UART_BASE, UART_RXCTRL) |= 1;

    if (earth->platform == ARTY) {
        /* UART0 send/recv are mapped to GPIO pin16 and pin17 */
        REGW(GPIO0_BASE, GPIO0_IOF_ENABLE) |= (1 << 16) | (1 << 17);
    }

    /* Increase UART Write Watermark to 3 */
    REGW(UART_BASE, UART_TXCTRL) |= 0x30000;

    /* Enable UART Interrupts for Reads */
    REGW(UART_BASE, UART_IE) |= 2;
}

int uart_pend_intr() {
    return REGW(UART_BASE, UART_IP);
}

void uart_txen() {
    REGW(UART_BASE, UART_IE) |= 0x1;
}

void uart_txdis() {
    REGW(UART_BASE, UART_IE) &= ~(0x1);
}

int uart_getc(int *c) {
    int ch = REGW(UART_BASE, UART_RXDATA);
    return *c = (ch & (1 << 31)) ? -1 : (ch & 0xFF); /* Bit 31 Indicates if Empty */
}

int uart_putc(int c) {
    int is_full = (REGW(UART_BASE, UART_TXDATA) & (1 << 31));

    if (is_full) return -1;

    REGW(UART_BASE, UART_TXDATA) = c;
    return 0;
}
