/* SPDX-License-Identifier: MIT */
/* Final ABI leaf fixture. No privileged state restoration or Uke device. */
.text
.global _start
_start:
  adr x4, context
  adr x0, marker
  mov x1, #0x11
  mov x2, #0x22
  mov x3, #0x33
  stp x0, x1, [x4]
  stp x2, x3, [x4, #16]
  mov x5, sp
  str x5, [x4, #32]
  adr x6, mock_kernel
  add x6, x6, #4
  str x6, [x4, #72]
#ifdef MZ_ENTRY
  mov x6, #1
#else
  mov x6, #0
#endif
  str x6, [x4, #80]
  mov x0, x4
  mov x1, #0
  mov x2, #0
  mov x3, #0
  sub sp, sp, #64
  b UkeResumeRawAndroid
mock_kernel:
  b failure // Resuming patched instruction 0 would re-enter firmware.
  b android
android:
  adr x6, marker
  cmp x0, x6
  b.ne failure
  cmp x1, #0x11
  b.ne failure
  cmp x2, #0x22
  b.ne failure
  cmp x3, #0x33
  b.ne failure
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
.bss
.p2align 4
context:
  .skip 96
