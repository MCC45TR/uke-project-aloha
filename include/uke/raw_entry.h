/* SPDX-License-Identifier: MIT */
#ifndef UKE_RAW_ENTRY_H
#define UKE_RAW_ENTRY_H
#include "uke/platform_boot.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Fixed 96-byte AArch64 assembly ABI. Capture does not enable caches/MMU or
 * change exception level. The SEC provider owns/admitted the storage first. */
typedef struct {
  uint64_t x[4];
  uint64_t stack, current_el, daif, sctlr, cntfrq, resume, replay_mz, reserved;
} UKE_RAW_ENTRY;
/* Narrow entry contract: non-secure EL1/EL2, IRQs masked, MMU and D-cache off,
 * standard raw Linux arguments. Unknown entry states stay on stock Android.
 * This check proves no board ownership/secure state/DMA/restore readiness. */
UKE_STATUS uke_sec_entry_admit(const UKE_RAW_ENTRY *entry, UKE_RANGE kernel,
                              UKE_RANGE fdt);
/* Called with original x0-x3 intact and x4 pointing to admitted identity-mapped
 * storage. resume/replay fields are filled and verified by the entry provider. */
/* UkeCaptureRawEntry is an assembly-only symbol: no C-callable prototype,
 * since a C prologue cannot preserve the original raw-entry stack. */
/* Only AFTER actual EFI exit and platform restoration of EL, MMU/caches,
 * interrupts, timers, coherency and DMA. No EFI/protocol dependency. */
void UkeResumeRawAndroid(const UKE_RAW_ENTRY *entry);
#ifdef __cplusplus
}
#endif
#endif
