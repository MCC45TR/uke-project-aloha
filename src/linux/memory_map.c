/* SPDX-License-Identifier: MIT */
#include "uke/linux_handoff.h"
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint64_t le64(const uint8_t *p) {
  return (uint64_t)le32(p) | ((uint64_t)le32(p + 4) << 32);
}
static int extent(const uint8_t *p, uint64_t *start, uint64_t *end) {
  uint64_t pages = le64(p + 24);
  *start = le64(p + 8);
  if (le32(p) > 15 || (*start & 4095) || !pages || pages > UINT64_MAX / 4096 ||
      *start > UINT64_MAX - pages * 4096) return 0;
  *end = *start + pages * 4096;
  return 1;
}
int uke_memory_map_check(const void *map, size_t bytes, size_t stride,
                         uint32_t version, UKE_MEMORY_INVENTORY *result) {
  const uint8_t *data = (const uint8_t *)map;
  size_t count, i, j;
  UKE_MEMORY_INVENTORY inventory = {0, 0, 0};
  if (!result) return 0;
  *result = inventory;
  if (!data || version != 1 || stride < 40 || stride > 256 || (stride & 7) ||
      !bytes || bytes % stride) return 0;
  count = bytes / stride;
  if (count > 2048) return 0;
  for (i = 0; i < count; ++i) {
    const uint8_t *p = data + i * stride;
    uint64_t start, end;
    if (!extent(p, &start, &end)) return 0;
    for (j = 0; j < i; ++j) {
      uint64_t other_start, other_end;
      if (!extent(data + j * stride, &other_start, &other_end)) return 0;
      if (start < other_end && other_start < end) return 0;
    }
    if (le32(p) == 7) {
      if (inventory.conventional_bytes > UINT64_MAX - (end - start)) return 0;
      inventory.conventional_bytes += end - start;
    }
    if (le64(p + 32) & ((uint64_t)1 << 63)) ++inventory.runtime_descriptors;
  }
  if (!inventory.conventional_bytes) return 0;
  inventory.descriptor_count = (uint32_t)count;
  *result = inventory;
  return 1;
}
