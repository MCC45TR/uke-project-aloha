/* SPDX-License-Identifier: MIT */
#include "uke/platform_boot.h"
int uke_range_valid(UKE_RANGE r) { return r.bytes && r.base <= UINT64_MAX - r.bytes; }
int uke_range_contains(UKE_RANGE a, UKE_RANGE b) {
  return uke_range_valid(a) && uke_range_valid(b) && b.base >= a.base &&
      b.bytes <= a.bytes && b.base - a.base <= a.bytes - b.bytes;
}
int uke_range_overlap(UKE_RANGE a, UKE_RANGE b) {
  return uke_range_valid(a) && uke_range_valid(b) && a.base < b.base + b.bytes && b.base < a.base + a.bytes;
}
static int inside_ram(const UKE_MEMORY_INPUT *m, UKE_RANGE r) {
  size_t i;
  for (i = 0; i < m->ram_count; ++i) if (uke_range_contains(m->ram[i], r)) return 1;
  return 0;
}
static int permitted(const UKE_MEMORY_INPUT *m, UKE_RANGE r) {
  size_t i;
  if (!uke_range_contains(m->allowed, r) || !inside_ram(m, r) ||
      uke_range_overlap(r, m->kernel) || uke_range_overlap(r, m->fdt) ||
      uke_range_overlap(r, m->initrd) || uke_range_overlap(r, m->entry_context)) return 0;
  for (i = 0; i < m->reserved_count; ++i) if (uke_range_overlap(r, m->reserved[i])) return 0;
  return 1;
}
UKE_STATUS uke_prepi_memory_plan(const UKE_MEMORY_INPUT *m, uint64_t fd,
                                uint64_t heap, uint64_t stack, UKE_MEMORY_PLAN *out) {
  UKE_MEMORY_PLAN candidate;
  UKE_RANGE all;
  size_t i, j;
  uint64_t base, total;
  if (!out) return UKE_INVALID_ARGUMENT;
  *out = (UKE_MEMORY_PLAN){0};
  if (!m || !m->complete_ownership || !m->ram || !m->ram_count ||
      m->ram_count > UKE_MAX_RAM_RANGES || m->reserved_count > UKE_MAX_RESERVED_RANGES ||
      (m->reserved_count && !m->reserved) || !uke_range_valid(m->allowed) ||
      !uke_range_valid(m->kernel) || !uke_range_valid(m->fdt) ||
      !uke_range_valid(m->entry_context) || (m->initrd.bytes && !uke_range_valid(m->initrd)) ||
      !fd || !heap || !stack || (fd | heap | stack) % 4096 != 0 ||
      fd > UINT64_MAX - heap || fd + heap > UINT64_MAX - stack)
    return UKE_INVALID_ARGUMENT;
  for (i = 0; i < m->ram_count; ++i) {
    if (!uke_range_valid(m->ram[i])) return UKE_INVALID_ARGUMENT;
    for (j = 0; j < i; ++j) if (uke_range_overlap(m->ram[i], m->ram[j])) return UKE_INVALID_ARGUMENT;
  }
  for (i = 0; i < m->reserved_count; ++i) if (!uke_range_valid(m->reserved[i])) return UKE_INVALID_ARGUMENT;
  if (!inside_ram(m, m->kernel) || !inside_ram(m, m->fdt) || !inside_ram(m, m->entry_context) ||
      (m->initrd.bytes && !inside_ram(m, m->initrd)) ||
      uke_range_overlap(m->kernel, m->fdt) || uke_range_overlap(m->kernel, m->initrd) ||
      uke_range_overlap(m->fdt, m->initrd) || uke_range_overlap(m->entry_context, m->fdt) ||
      uke_range_overlap(m->entry_context, m->initrd)) return UKE_INVALID_ARGUMENT;
  /* A saved context can reside in the expanded appended kernel allocation. */
  if (uke_range_overlap(m->entry_context, m->kernel) && !uke_range_contains(m->kernel, m->entry_context))
    return UKE_INVALID_ARGUMENT;
  total = fd + heap + stack;
  if (m->allowed.base > UINT64_MAX - 4095) return UKE_INVALID_ARGUMENT;
  base = (m->allowed.base + 4095) & ~((uint64_t)4095);
  all = (UKE_RANGE){base, total};
  if (!permitted(m, all)) return UKE_BACKEND_UNAVAILABLE;
  candidate.firmware = (UKE_RANGE){base, fd};
  candidate.heap = (UKE_RANGE){base + fd, heap};
  candidate.stack = (UKE_RANGE){base + fd + heap, stack};
  *out = candidate; return UKE_OK;
}
