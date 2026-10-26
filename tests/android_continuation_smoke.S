/* SPDX-License-Identifier: MIT */
/* A Linux-ABI user-mode fixture, not a bootloader/EL/MMU/Uke hardware test. */
.section .text,"ax"
.global _start
_start:
  adr x0, marker
  mov x1, #0
  mov x2, #0
  mov x3, #0
  mov x5, sp
  b mock_kernel
.align 12
mock_kernel:
  b selector
  b android
  .zero 56
android:
  adr x6, marker
  cmp x0, x6
  b.ne failure
  cbnz x1, failure
  cbnz x2, failure
  cbnz x3, failure
  mov x6, sp
  cmp x5, x6
  b.ne failure
  mov x0, #0
  mov x8, #93
  svc #0
failure:
  mov x0, #1
  mov x8, #93
  svc #0
marker:
  .ascii "fixture-fdt-sentinel"
  .org mock_kernel + 8192
selector:
  .incbin "AndroidContinuation.bin", 0, 24
  .quad selector - mock_kernel
#ifdef MZ_ENTRY
  .quad 4
#else
  .quad 64
#endif
  .incbin "AndroidContinuation.bin", 40, 108
#ifdef MZ_ENTRY
  .inst 0xfa405a4d
#else
  nop
#endif
  .incbin "AndroidContinuation.bin", 152, 104
