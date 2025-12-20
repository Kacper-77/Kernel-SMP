OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(kernel_main)

SECTIONS {
  . = 0x00100000;

  .text : {
    KEEP(*(.text.entry))
    *(.text*)
  }

  .rodata : { *(.rodata*) }
  .data   : { *(.data*) }
  .bss    : { *(COMMON) *(.bss*) }
}
