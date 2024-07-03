/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: a simple tty device driver
 * uart_getc() and uart_putc() are implemented in bus_uart.c
 * printf-related functions are linked from the compiler's C library
 */

#define LIBC_STDIO
#include "egos.h"
#include "tty.h"
#include <stdio.h>
#include <stdarg.h>

struct tty_queue {
    char buff[TTY_QUEUE_SIZE];
    int size;
    int head;
    int tail;
};

struct tty_queue tty_readq;
struct tty_queue tty_writeq;

#define rx_head tty_readq.head
#define rx_tail tty_readq.tail
#define rx_size tty_readq.size

#define tx_head tty_writeq.head
#define tx_tail tty_writeq.tail
#define tx_size tty_writeq.size

int uart_pend_intr();
int uart_getc(int *c);
int uart_putc(int c);
void uart_init(long baud_rate);

void uart_txen();
void uart_txdis();

static int c;

void tty_buff_init() {
    for (int i = 0; i < TTY_QUEUE_SIZE; i++)
        tty_readq.buff[i] = tty_writeq.buff[i] = 0;

    rx_size = tx_size = 0;
    rx_head = tx_head = 0;
    rx_tail = tx_tail = 0;
}

int kernel_tty_write(char *buf, uint len) {
    int rc;
    for (int i = 0; i < len; i++) {
        do {
            rc = uart_putc(buf[i]);
        } while (rc == -1);
    }
    return len; // Must Return Length of Message Written to _write()
}

void tty_write_uart() {
    int rc;

    while (tx_size > 0) {
        rc = uart_putc((int)tty_writeq.buff[tx_head]);

        if (rc == -1) {
            uart_txen(); // Tx Full and Buffer Non-Empty, Enable Interrupts
            return;
        }

        tx_head = (tx_head + 1) % TTY_QUEUE_SIZE;
        tx_size--;
    }

    uart_txdis(); // Buffer Empty, Disable Interrupts
}

void tty_write_buff(char *buf, uint len) {
    /* Only Called when Entire Input Fits in Buff */
    if (len + tx_size > TTY_QUEUE_SIZE) return;

    for (int i = 0; i < len; i++) {
        tty_writeq.buff[tx_tail] = buf[i];
        tx_tail = (tx_tail + 1) % TTY_QUEUE_SIZE;
    }

    tx_size += len;
}

int tty_write(char *buf, uint len) {
    if (len > TTY_QUEUE_SIZE)           return -2; // Error, Request will never succeed
    if (len + tx_size > TTY_QUEUE_SIZE) return -1; // Failure, Retry on UART Interrupt

    /* Write Contents into Kernel Buffer */
    tty_write_buff(buf, len);
    /* Write Kernel Buffer into UART */
    tty_write_uart();

    return 0;
}

int tty_read_uart() {
    if (uart_getc(&c) == -1) return -1;

    do {
        tty_readq.buff[rx_tail] = (char)c;

        if (rx_size < TTY_QUEUE_SIZE) {
            rx_tail = (rx_tail + 1) % TTY_QUEUE_SIZE;
            rx_size++;
        }

        /* Return to Kernel To Kill Killable Processes */
        if (c == SPECIAL_CTRL_C) return RET_SPECIAL_CHAR;

    } while (uart_getc(&c) != -1);

    return 0;
}

int tty_read(char *c) {
    if (rx_size == 0) return -1;

    *c = (char)tty_readq.buff[rx_head];

    rx_head = (rx_head + 1) % TTY_QUEUE_SIZE;
    rx_size--;
    return 0;
}

int tty_read_tail(char *c) {
    if (rx_size == 0) return -1;
    if (rx_tail == 0) rx_tail = TTY_QUEUE_SIZE;
    rx_tail--;

    *c = tty_readq.buff[rx_tail];
    tty_readq.buff[rx_tail] = 0;
    rx_size--;

    return 0;
}

void kernel_tty_read(char *buf, uint len) {
    for (int i = 0; i < len; i++) {
        while (uart_getc(&c) == -1);
        buf[i] = (char)c;
    }
}

#define LOG(x, y)           \
    printf(x);              \
    va_list args;           \
    va_start(args, format); \
    vprintf(format, args);  \
    va_end(args);           \
    printf(y);

int tty_printf(const char *format, ...) {
    LOG("", "")
    fflush(stdout);
}

int tty_info(const char *format, ...) { LOG("[INFO] ", "\r\n") }

int tty_fatal(const char *format, ...) {
    LOG("\x1B[1;31m[FATAL] ", "\x1B[1;0m\r\n") /* red color */
    while (1);
}

int tty_success(const char *format, ...) {
    LOG("\x1B[1;32m[SUCCESS] ", "\x1B[1;0m\r\n") /* green color */
}

int tty_critical(const char *format, ...) {
    LOG("\x1B[1;33m[CRITICAL] ", "\x1B[1;0m\r\n") /* yellow color */
}

int tty_intr() {
    int rc, ip;
    ip = uart_pend_intr();

    if (ip & UART_RX_INTR)
        rc = tty_read_uart();
    if (ip & UART_TX_INTR) 
        tty_write_uart();
    return rc;
}

void tty_init() {
    /* 115200 is the UART baud rate */
    uart_init(115200);

    /* Wait for the tty device to be ready */
    for (int c = 0; c != -1; uart_getc(&c));

    tty_buff_init();

    earth->tty_read = tty_read;
    earth->tty_read_tail = tty_read_tail;
    earth->kernel_tty_read = kernel_tty_read;

    earth->tty_write = tty_write;
    earth->kernel_tty_write = kernel_tty_write;

    earth->tty_printf = tty_printf;
    earth->tty_info = tty_info;
    earth->tty_fatal = tty_fatal;
    earth->tty_success = tty_success;
    earth->tty_critical = tty_critical;
}
