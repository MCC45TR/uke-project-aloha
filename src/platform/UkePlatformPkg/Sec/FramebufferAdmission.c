/* SPDX-License-Identifier: MIT */
#include "uke/sec_probe.h"
#include <libfdt.h>
#include <string.h>

#define FB_BYTES (3200ULL * 2136 * 4)
static int range(uint64_t base, uint64_t size) { return base && size && size <= UINT64_MAX - base; }
static int overlap(uint64_t a, uint64_t an, uint64_t b, uint64_t bn) {
  return a < b + bn && b < a + an;
}
static int cells(const void *fdt, int node) {
  return fdt_address_cells(fdt, node) == 2 && fdt_size_cells(fdt, node) == 2;
}
static uint64_t number(const fdt32_t *p) {
  return ((uint64_t)fdt32_to_cpu(p[0]) << 32) | fdt32_to_cpu(p[1]);
}

int uke_sec_admit_framebuffer(const void *fdt, size_t bytes, uint64_t kernel,
                        uint64_t kernel_bytes, struct uke_sec_framebuffer *out) {
  int node, length, memory = 0, splash, reserved;
  const fdt32_t *reg;
  const char *model;
  uint64_t base, size, fdt_address = (uint64_t)(uintptr_t)fdt;
  if (!out) return 0;
  *out = (struct uke_sec_framebuffer){0, 0};
  if (!fdt || bytes < sizeof(struct fdt_header) || bytes > 2 * 1024 * 1024 ||
      !range(fdt_address, bytes) || !range(kernel, kernel_bytes) ||
      fdt_check_full(fdt, bytes) || !cells(fdt, 0) ||
      fdt_node_check_compatible(fdt, 0, "qcom,cliffs-mtp") ||
      fdt_node_check_compatible(fdt, 0, "qcom,cliffs")) return 0;
  model = fdt_getprop(fdt, 0, "model", &length);
  if (!model || length != sizeof("Qualcomm Technologies, Inc. Uke based on SM8635") ||
      memcmp(model, "Qualcomm Technologies, Inc. Uke based on SM8635", (size_t)length)) return 0;
  reserved = fdt_path_offset(fdt, "/reserved-memory");
  splash = fdt_path_offset(fdt, "/reserved-memory/splash_region");
  if (reserved < 0 || splash < 0 || !cells(fdt, reserved)) return 0;
  reg = fdt_getprop(fdt, reserved, "ranges", &length);
  if (!reg || length != 0) return 0;
  reg = fdt_getprop(fdt, splash, "reg", &length);
  if (!reg || length != 16 || fdt_getprop(fdt, splash, "reusable", 0)) return 0;
  base = number(reg); size = number(reg + 2);
  if (!range(base, size) || (base & 4095) || size < FB_BYTES || size > 128 * 1024 * 1024 ||
      overlap(base, size, kernel, kernel_bytes) || overlap(base, size, fdt_address, bytes)) return 0;
  fdt_for_each_subnode(node, fdt, 0) {
    const char *type = fdt_getprop(fdt, node, "device_type", &length);
    if (!type || length != 7 || memcmp(type, "memory", 7)) continue;
    reg = fdt_getprop(fdt, node, "reg", &length);
    if (!reg || length <= 0 || length % 16) return 0;
    for (int i = 0; i < length / 4; i += 4) {
      uint64_t start = number(reg + i), count = number(reg + i + 2);
      /* The retained OEM map contains zero-length holes. */
      if (!count) continue;
      if (!range(start, count)) return 0;
      if (base >= start && base + size <= start + count) memory = 1;
    }
  }
  if (!memory) return 0;
  fdt_for_each_subnode(node, fdt, reserved) {
    if (node == splash) continue;
    reg = fdt_getprop(fdt, node, "reg", &length);
    if (!reg) continue; /* Dynamic reservations have no assigned address yet. */
    if (length <= 0 || length % 16) return 0;
    for (int i = 0; i < length / 4; i += 4) {
      uint64_t start = number(reg + i), count = number(reg + i + 2);
      if (!count) continue;
      if (!range(start, count) || overlap(base, size, start, count)) return 0;
    }
  }
  for (int i = 0; i < fdt_num_mem_rsv(fdt); ++i) {
    uint64_t start, count;
    if (fdt_get_mem_rsv(fdt, i, &start, &count) || !range(start, count) ||
        overlap(base, size, start, count)) return 0;
  }
  out->address = base; out->bytes = FB_BYTES;
  return 1;
}
