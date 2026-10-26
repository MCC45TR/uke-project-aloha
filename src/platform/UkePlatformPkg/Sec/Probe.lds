/* SPDX-License-Identifier: MIT */
ENTRY(_start)
SECTIONS {
  . = 0;
  .text : { KEEP(*(.text.entry)) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss : { *(.bss*) *(COMMON) }
  ASSERT(SIZEOF(.bss) == 0, "probe must not depend on uninitialized BSS")
  .stack : { . = ALIGN(16); BYTE(0); . += 32767; . = ALIGN(4096); _probe_end = .; }
  /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
