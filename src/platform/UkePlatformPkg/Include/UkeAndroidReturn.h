/* SPDX-License-Identifier: MIT */
#ifndef UKE_ANDROID_RETURN_H
#define UKE_ANDROID_RETURN_H
#include <Uefi.h>
#include "uke/platform_boot.h"
/* The board provider must validate retained kernel/FDT/initrd, physical entry
 * context and restoration code BEFORE publishing AndroidReady. This adapter
 * performs real boot-services exit; it never invents CPU/DMA restoration. */
typedef struct {
  EFI_BOOT_SERVICES *services;
  EFI_HANDLE image;
  VOID *board_context;
  BOOLEAN (EFIAPI *RestoreReady)(VOID *context);
  UKE_STATUS (EFIAPI *RestoreAndJump)(VOID *context);
  EFI_MEMORY_DESCRIPTOR *map;
  UINTN capacity, map_bytes, key, descriptor_bytes;
  UINT32 descriptor_version;
  BOOLEAN prepared, exit_attempted, exited, poisoned;
} UKE_ANDROID_RETURN;
int UkeAndroidReturnReady(void *context);
UKE_STATUS UkeAndroidReturnPrepare(void *context);
UKE_STATUS UkeAndroidExitServices(void *context);
UKE_STATUS UkeAndroidRestoreAndJump(void *context);
/* Only before any ExitBootServices attempt. After an attempt, even a failed
 * one, EFI services may already be partially shut down: do not free/retry. */
EFI_STATUS UkeAndroidReturnRelease(UKE_ANDROID_RETURN *context);
#endif
