/* SPDX-License-Identifier: MIT */
#ifndef UKE_LINUX_HANDOFF_H
#define UKE_LINUX_HANDOFF_H
#ifdef UKE_EDK2
#include <Base.h>
typedef UINT8 uint8_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef UINTN size_t;
#define UINT64_MAX MAX_UINT64
#else
#include <stddef.h>
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
  uint32_t descriptor_count;
  uint64_t conventional_bytes;
  uint32_t runtime_descriptors;
} UKE_MEMORY_INVENTORY;
/* Inspect the standard little-endian EFI descriptor ABI, without changing it.
 * The caller owns memory-map lifetime and firmware/profile admission. No RAM
 * addresses, carveouts or permission to execute are synthesized by this API. */
int uke_memory_map_check(const void *map, size_t bytes, size_t descriptor_bytes,
                         uint32_t descriptor_version, UKE_MEMORY_INVENTORY *result);
#ifdef __cplusplus
}
#endif
#endif
