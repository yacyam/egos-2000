OUTPUT_ARCH("riscv")

ENTRY(app_entry)

MEMORY
{
    ram (arw!xi) : ORIGIN = 0x08005000, LENGTH = 0x5000
}

PHDRS
{
    ram PT_LOAD;
}

SECTIONS
{
    .text : ALIGN(0x1000) {
        *(.text .text.*)
    } >ram :ram

    .rodata : ALIGN(0x1000) {
        *(.rdata)
        *(.rodata .rodata.*)
        . = ALIGN(8);
        *(.srodata .srodata.*)
    } >ram :ram

    .data : ALIGN(0x1000) {
        *(.data .data.*)
        . = ALIGN(8);
        *(.sdata .sdata.* .sdata2.*)
    } >ram :ram

    .bss (NOLOAD) : ALIGN(0x1000) {
        *(.sbss*)
        *(.bss .bss.*)
        *(COMMON)
    } >ram :ram

    .heap (NOLOAD) : ALIGN(8) {
          PROVIDE( __heap_start = . );
    } >ram :ram

    PROVIDE( __heap_end = 0x0800b000 );
}
