/* SPDX-License-Identifier: MIT */
#include "uke/raw_entry.h"
_Static_assert(sizeof(UKE_RAW_ENTRY) == 96, "assembly context ABI");
UKE_STATUS uke_sec_entry_admit(const UKE_RAW_ENTRY *e, UKE_RANGE kernel, UKE_RANGE fdt) {
  if (!e || !uke_range_valid(kernel) || !uke_range_valid(fdt)) return UKE_INVALID_ARGUMENT;
  if ((e->current_el != 4 && e->current_el != 8) || (e->daif & 0x3c0) != 0x3c0 ||
      (e->sctlr & 5) || !e->cntfrq || e->cntfrq > 0xffffffffu ||
      !e->stack || (e->stack & 15) || (e->x[0] & 7) || e->x[0] != fdt.base ||
      e->x[1] || e->x[2] || e->x[3] || fdt.bytes > 2 * 1024 * 1024 ||
      !uke_range_contains(kernel, (UKE_RANGE){e->resume, 4}) || (e->resume & 3) ||
      e->resume != kernel.base + 4 || e->replay_mz > 1 || e->reserved ||
      uke_range_overlap(kernel, fdt)) return UKE_BACKEND_UNAVAILABLE;
  return UKE_OK;
}
