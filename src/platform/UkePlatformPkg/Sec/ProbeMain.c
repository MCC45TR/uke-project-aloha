/* SPDX-License-Identifier: MIT */
#include "uke/sec_probe.h"
#include <libfdt.h>

void UkeSecProbeMain(const void *fdt, const void *header, uint64_t kernel) {
  struct uke_sec_framebuffer fb;
  const uint64_t *metadata = header;
  size_t bytes;
  if (!fdt || ((uintptr_t)fdt & 7) || fdt_check_header(fdt)) return;
  bytes = fdt_totalsize(fdt);
  /* The expanded Image owns the probe and its private stack. */
  if (!uke_sec_admit_framebuffer(fdt, bytes, kernel,
      metadata[3] + ((const uint32_t *)header)[5], &fb)) return;
  volatile uint32_t *pixels = (volatile uint32_t *)(uintptr_t)fb.address;
  /* A small, distinct marker survives until the Linux console takes over. */
  for (unsigned y = 0; y < 128; ++y)
    for (unsigned x = 0; x < 768; ++x)
      pixels[y * 3200 + x] = y < 16 ? 0xff00ff88U : 0xff102c44U;
  static const unsigned char glyphs[][5] = {
    {0x7e,0x09,0x09,0x09,0x7e}, /* A */ {0x7f,0x49,0x49,0x49,0x41}, /* E */
    {0x7f,0x08,0x08,0x08,0x7f}, /* H */ {0x41,0x41,0x7f,0x41,0x41}, /* I */
    {0x7f,0x08,0x14,0x22,0x41}, /* K */ {0x7f,0x40,0x40,0x40,0x40}, /* L */
    {0x7f,0x02,0x04,0x08,0x7f}, /* N */ {0x3e,0x41,0x41,0x41,0x3e}, /* O */
    {0x7f,0x3f,0x00,0x3f,0x7f}, /* U */ {0x63,0x14,0x08,0x14,0x63}  /* X */
  };
  static const char alphabet[] = "AEHIKLNOUX";
  static const char text[] = "UKE ALOHA  LINUX";
  for (unsigned n = 0; n < sizeof(text) - 1; ++n) {
    unsigned g = 0;
    while (g < sizeof(alphabet) - 1 && alphabet[g] != text[n]) ++g;
    if (g == sizeof(alphabet) - 1) continue;
    for (unsigned x = 0; x < 5; ++x) for (unsigned y = 0; y < 7; ++y)
      if (glyphs[g][x] & (1U << y))
        for (unsigned dy = 0; dy < 6; ++dy) for (unsigned dx = 0; dx < 6; ++dx)
          pixels[(36 + y * 6 + dy) * 3200 + 24 + n * 42 + x * 6 + dx] = 0xff00ff88U;
  }
  __asm__ volatile("dsb sy" ::: "memory");
}
