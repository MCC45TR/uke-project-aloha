/* SPDX-License-Identifier: MIT */
/* Final raw ABI leaf after platform restoration. It does not turn an EFI
 * callback into a safe hardware backend. Only x0-x3/SP are raw boot arguments;
 * x15-x17 are scratch. resume points to the preserved original instruction 1. */
.text
.p2align 2
.global UkeResumeRawAndroid
UkeResumeRawAndroid:
  mov x16, x0
  ldr x17, [x16, #72]
  ldr x15, [x16, #32]
  mov sp, x15
  ldr x15, [x16, #80]
  ldp x0, x1, [x16]
  ldp x2, x3, [x16, #16]
  cbz x15, 1f
  .inst 0xfa405a4d  // Replay the recognized stock ARM64 MZ instruction.
1:
  br x17
