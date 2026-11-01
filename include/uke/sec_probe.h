/* SPDX-License-Identifier: MIT */
#ifndef UKE_SEC_PROBE_H
#define UKE_SEC_PROBE_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct uke_sec_framebuffer { uint64_t address, bytes; };
/* Read-only admission; no FDT changes or framebuffer access. */
int uke_sec_admit_framebuffer(const void *fdt, size_t bytes, uint64_t kernel,
                        uint64_t kernel_bytes, struct uke_sec_framebuffer *out);
void UkeSecProbeMain(const void *fdt, const void *header, uint64_t kernel);
#ifdef __cplusplus
}
#endif
#endif
