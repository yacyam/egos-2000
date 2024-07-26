OUTPUT_ARCH("riscv")

ENTRY(loader_entry)

MEMORY 
{
  ram (rwx!ia) : ORIGIN = 0x80030000, LENGTH = 0x1000
}

SECTIONS
{
  .text : ALIGN(8) {
    *(.text.enter .text.*)
  } >ram

  .rodata : ALIGN(8) {
      *(.rdata)
      *(.rodata .rodata.*)
  } >ram

  .data : ALIGN(8) {
      *(.data .data.*)
  } >ram

  .bss (NOLOAD): ALIGN(8) {
      *(.sbss*)
      *(.bss .bss.*)
      *(COMMON)
  } >ram
}