OUTPUT_ARCH("riscv")

ENTRY(app_entry)

PHDRS
{
    ram PT_LOAD;
}

SECTIONS
{
    .text : ALIGN(0x1000) {
        *(.text .text.*)
    } :ram

    .rodata : ALIGN(0x1000) {
        *(.rdata)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        *(.srodata .srodata.*)
    } :ram

    .data : ALIGN(0x1000) {
        *(.data .data.*)
        . = ALIGN(8);
        *(.sdata .sdata.* .sdata2.*)
    } :ram

    .bss (NOLOAD) : ALIGN(0x1000) {
        *(.sbss*)
        *(.bss .bss.*)
        *(COMMON)
    } :ram

    .heap (NOLOAD) : ALIGN(8) {
          PROVIDE( __heap_start = . );
    } :ram

    PROVIDE( __heap_end = __heap_start + 0x1000 );
}
