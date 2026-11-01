/* SPDX-License-Identifier: MIT */
/* Firmware-only entry helper, NOT an Android image selector. x4 must already
 * identify admitted writable context; this function cannot discover ownership.
 * No stack, firmware address, pstore, MMIO or exception-state mutation. */
.text
.p2align 2
.global UkeCaptureRawEntry
UkeCaptureRawEntry:
  stp x0, x1, [x4]
  stp x2, x3, [x4, #16]
  mov x16, sp
  mrs x17, CurrentEL
  stp x16, x17, [x4, #32]
  mrs x16, DAIF
  cmp x17, #8
  b.eq 2f
  cmp x17, #4
  b.ne 3f
  mrs x17, SCTLR_EL1
  b 4f
2:
  mrs x17, SCTLR_EL2
  b 4f
3:
  mov x17, #-1  // An unsupported EL never passes admission.
4:
  stp x16, x17, [x4, #48]
  mrs x16, CNTFRQ_EL0
  str x16, [x4, #64]
  ret
