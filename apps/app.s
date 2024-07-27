/*
 * (C) 2022, Cornell University
 * All rights reserved.
 */

/* Author: Yunhao Zhang
 * Description: initialize the stack pointer and call application main()
 */
    .section .text
    .global app_entry
app_entry:
    call main
    call exit
