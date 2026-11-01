/* SPDX-License-Identifier: MIT */
/* Synthetic QEMU virt RAM fixture; these addresses are never Uke defaults. */
.text
.global _start
_start:
#ifdef MZ_ENTRY
  .word 0xfa405a4d
  b original_entry
#else
  b original_entry
  nop
#endif
  .quad 0x80000
  .quad 8192
  .quad 0
  .quad 0, 0, 0
  .word 0x644d5241
  .word 0
original_entry:
  cbz x0, failed
  cbnz x1, failed
  cbnz x2, failed
  cbnz x3, failed
  mov x4, sp
  cbnz x4, failed
  ldr w4, [x0]
  mov w5, #0x0dd0
  movk w5, #0xedfe, lsl #16
  cmp w4, w5
  b.ne failed
  mov x4, #0x50000000
  ldr w4, [x4]
#ifdef EXPECT_BYPASS
  mov w5, #0
#else
  mov w5, #0xff88
  movk w5, #0xff00, lsl #16
#endif
  cmp w4, w5
  b.ne failed
  mrs x4, CurrentEL
  cmp x4, #4
  adr x1, el1_text
  b.eq print_el
  cmp x4, #8
  b.ne failed
  adr x1, el2_text
print_el:
  mov x0, #4
  hlt #0xf000
  mov x0, #4
  adr x1, passed_text
  hlt #0xf000
  mov x0, #0x20
  adr x1, passed_exit
  hlt #0xf000
failed:
  mov x0, #4
  adr x1, failed_text
  hlt #0xf000
  mov x0, #0x20
  adr x1, failed_exit
  hlt #0xf000
  b failed
.balign 8
passed_exit: .quad 0x20026, 0
failed_exit: .quad 0x20026, 1
#ifdef EXPECT_BYPASS
passed_text: .asciz "UKE-SEC-QEMU: rejected FDT and original x0-x3/SP resume passed\n"
#else
passed_text: .asciz "UKE-SEC-QEMU: framebuffer and original x0-x3/SP resume passed\n"
#endif
failed_text: .asciz "UKE-SEC-QEMU: FAIL\n"
el1_text: .asciz "UKE-SEC-QEMU: actual EL1\n"
el2_text: .asciz "UKE-SEC-QEMU: actual EL2\n"
.org 4096
