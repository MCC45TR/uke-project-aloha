/* SPDX-License-Identifier: MIT */
.section .text.entry,"ax"
.global _start
_start:
  b entry
  nop
  .ascii "UKESPR01"
  .word 1
  .word _probe_end - _start
selector_offset: .quad 0
resume_offset: .quad 0
original_kernel_size: .quad 0
original_image_size: .quad 0
original_code0: .word 0
  .word replay_code0 - _start
  .zero 64
entry:
  /* No EL, translation, timer, interrupt or controller state is changed. */
  mrs x4, CurrentEL
  cmp x4, #4
  b.eq el1
  cmp x4, #8
  b.ne resume
  mrs x4, sctlr_el2
  b state
el1:
  mrs x4, sctlr_el1
state:
  tbnz x4, #0, resume
  tbnz x4, #2, resume
  tbnz x4, #25, resume
  mrs x4, daif
  and x4, x4, #0xc0
  cmp x4, #0xc0
  b.ne resume
  adr x4, _start
  tst x4, #4095
  b.ne resume
  orr x4, x1, x2
  orr x4, x4, x3
  cbnz x4, resume
  cbz x0, resume
  tst x0, #7
  b.ne resume
  ldr w4, [x0]
  rev w4, w4
  mov w5, #0xfeed
  movk w5, #0xd00d, lsl #16
  cmp w4, w5
  b.ne resume
  ldr w4, [x0, #4]
  rev w4, w4
  cmp w4, #40
  b.lo resume
  mov w5, #0x200000
  cmp w4, w5
  b.hi resume
  adds x5, x0, x4
  b.cs resume
  adr x6, _start
  adr x7, _probe_end
  cmp x0, x7
  b.hs stack_ready
  cmp x5, x6
  b.hi resume
stack_ready:
  mov x9, sp
  adr x10, _probe_end
  mov sp, x10
  stp x0, x1, [sp, #-48]!
  stp x2, x3, [sp, #16]
  str x9, [sp, #32]
  adr x1, _start
  ldr x2, selector_offset
  sub x2, x1, x2
  bl UkeSecProbeMain
  ldp x0, x1, [sp]
  ldp x2, x3, [sp, #16]
  ldr x9, [sp, #32]
  mov sp, x9
resume:
  adr x16, _start
  ldr x17, selector_offset
  sub x16, x16, x17
  ldr x17, resume_offset
  add x16, x16, x17
replay_code0:
  nop
  br x16
