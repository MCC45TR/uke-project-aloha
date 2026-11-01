/* SPDX-License-Identifier: MIT */
/* Development-only: resume Android directly. No firmware, reset, MMIO, stack,
 * storage, ESP or pstore access. x0-x3 and SP remain untouched. x16/x17 scratch
 * follow the raw Linux boot ABI. See docs/KERNEL-ENTRY-DEVELOPMENT.md. */
.section .text,"ax"
.global _start
_start:
  b entry
  nop
  .ascii "UKEAND01"
  .word 1
  .word 256
selector_offset: .quad 0
resume_offset: .quad 0
original_kernel_size: .quad 0
original_image_size: .quad 0
original_code0: .word 0
mode: .word 0
original_boot_sha: .zero 32
template_sha: .zero 32
entry:
  adr x16, _start
  ldr x17, selector_offset
  sub x16, x16, x17
  ldr x17, resume_offset
  add x16, x16, x17
replay_code0:
  nop
  br x16
  .org 256
