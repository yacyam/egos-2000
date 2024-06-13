/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: RISC-V interrupt and exception handling
 */

#include "egos.h"

#define PLIC_BASE      0x0C000000
#define PLIC_PRIORITY  0x0
#define PLIC_ENABLES   (earth->platform == ARTY ? 0x2000 : 0x2080)
#define PLIC_THRESHOLD (earth->platform == ARTY ? 0x200000 : 0x201000)
#define PLIC_CLAIM     (earth->platform == ARTY ? 0x200004 : 0x201004) // ARTY still programmed with FE Chip, 1 core.

#define PLIC_UART_ID   (earth->platform == ARTY ? 3 : 4)
#define PLIC_SPI_ID    6 

/* These are two static variables storing
 * the addresses of the handler functions;
 * Initially, both variables are NULL */
static void (*kernel_entry)(uint, uint);
int kernel_entry_init(void (*new_entry)(uint, uint)) {
    kernel_entry = new_entry;
}

/* Both trap functions are defined in earth.S */
void trap_from_M_mode();
void trap_from_S_mode();

void trap_entry() {
    uint mcause;
    asm("csrr %0, mcause" : "=r"(mcause));
    kernel_entry(mcause & (1 << 31), mcause & 0x3FF);
}

void extr_enable(uint id) {
    REGW(PLIC_BASE, PLIC_PRIORITY + (4 * id))  |= 1;
    REGW(PLIC_BASE, PLIC_ENABLES) |= (1 << id);
}

void intr_init() {
    earth->kernel_entry_init = kernel_entry_init;

    /* Setup the interrupt/exception entry function */
    if (earth->translation == PAGE_TABLE) {
        asm("csrw mtvec, %0" ::"r"(trap_from_S_mode));
        INFO("Use direct mode and put the address of trap_entry_S_mode() to mtvec");
    } else {
        asm("csrw mtvec, %0" ::"r"(trap_from_M_mode));
        INFO("Use direct mode and put the address of trap_entry_M_mode() to mtvec");
    }

    /* Enable the machine-mode timer, software, and external interrupts */
    uint mstatus, mie;
    asm("csrr %0, mie" : "=r"(mie));
    asm("csrw mie, %0" ::"r"(mie | 0x888));
    asm("csrr %0, mstatus" : "=r"(mstatus));
    asm("csrw mstatus, %0" ::"r"(mstatus | 0x80));

    /* Enable Interrupts on PLIC for UART and SPI */
    REGW(PLIC_BASE, PLIC_THRESHOLD) = 0;
    extr_enable(PLIC_UART_ID);
    extr_enable(PLIC_SPI_ID);
}
