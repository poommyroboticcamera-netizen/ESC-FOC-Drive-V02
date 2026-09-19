ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
  RAM (rw) : ORIGIN = 0x20000000, LENGTH = 128K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
  .isr_vector : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  /* Fixed address for SWD access; mailbox is zeroed with the rest of BSS. */
  .bss (NOLOAD) : {
    . = ALIGN(4); _sbss = .;
    KEEP(*(.board_mailbox))
    *(.bss*) *(COMMON)
    . = ALIGN(4); _ebss = .;
  } > RAM
  .data : {
    . = ALIGN(4); _sdata = .;
    *(.data*)
    . = ALIGN(4); _edata = .;
  } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  ASSERT(SIZEOF(.bss) + SIZEOF(.data) < 120K, "Leave at least 8K stack")
  ASSERT(g_debug == ORIGIN(RAM), "Debug mailbox address changed")
}
