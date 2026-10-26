/* SPDX-License-Identifier: MIT */
#ifndef UKE_PLATFORM_BOOT_H
#define UKE_PLATFORM_BOOT_H
#include "uke/boot_core.h"
#ifdef __cplusplus
extern "C" {
#endif
#define UKE_MAX_RAM_RANGES 32u
#define UKE_MAX_RESERVED_RANGES 128u
#define UKE_MAX_QUIESCE_DRIVERS 32u
typedef struct { uint64_t base, bytes; } UKE_RANGE;
typedef struct {
  const UKE_RANGE *ram;
  size_t ram_count;
  const UKE_RANGE *reserved;
  size_t reserved_count;
  /* Explicit reviewed firmware-owned aperture, never all unspecified RAM. */
  UKE_RANGE allowed;
  UKE_RANGE kernel, fdt, initrd, entry_context;
  int complete_ownership;
} UKE_MEMORY_INPUT;
typedef struct { UKE_RANGE firmware, heap, stack; } UKE_MEMORY_PLAN;
/* Allocates nothing and performs no physical access. All output is zero on
 * failure. Called before any SEC/PrePi write/relocation into proposed regions. */
UKE_STATUS uke_prepi_memory_plan(const UKE_MEMORY_INPUT *input, uint64_t firmware_bytes,
                                uint64_t heap_bytes, uint64_t stack_bytes,
                                UKE_MEMORY_PLAN *plan);
int uke_range_valid(UKE_RANGE range);
int uke_range_contains(UKE_RANGE outer, UKE_RANGE inner);
int uke_range_overlap(UKE_RANGE first, UKE_RANGE second);

typedef UKE_STATUS (*UKE_QUIESCE)(void *context);
typedef struct { void *context; UKE_QUIESCE quiesce; int dirty; } UKE_DRIVER;
typedef enum {
  UKE_RETURN_IDLE, UKE_RETURN_QUIESCING, UKE_RETURN_PREPARING,
  UKE_RETURN_EXITING, UKE_RETURN_JUMPING, UKE_RETURN_FAILED
} UKE_RETURN_STAGE;
typedef struct {
  void *context;
  int (*ready)(void *context);
  UKE_STATUS (*prepare)(void *context);
  UKE_STATUS (*exit_services)(void *context);
  UKE_STATUS (*jump_android)(void *context);
} UKE_RETURN_BACKEND;
typedef struct {
  UKE_DRIVER drivers[UKE_MAX_QUIESCE_DRIVERS];
  size_t driver_count;
  UKE_RETURN_STAGE stage;
  UKE_STATUS failure;
} UKE_RETURN_TRANSACTION;
/* Drivers register before mutation and mark dirty before enabling DMA/MMIO.
 * No restart/retry after an uncertain quiesce or ExitBootServices outcome. */
UKE_STATUS uke_return_register(UKE_RETURN_TRANSACTION *transaction,
                               void *context, UKE_QUIESCE quiesce, size_t *index);
UKE_STATUS uke_return_mark_dirty(UKE_RETURN_TRANSACTION *transaction, size_t index);
UKE_STATUS uke_return_android(UKE_RETURN_TRANSACTION *transaction,
                              const UKE_RETURN_BACKEND *backend);
#ifdef __cplusplus
}
#endif
#endif
