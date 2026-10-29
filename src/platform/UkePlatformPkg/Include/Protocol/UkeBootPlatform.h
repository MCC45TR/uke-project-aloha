/* SPDX-License-Identifier: MIT */
#ifndef UKE_BOOT_PLATFORM_PROTOCOL_H
#define UKE_BOOT_PLATFORM_PROTOCOL_H
#include <Uefi.h>
#include <Protocol/DevicePath.h>
#include "uke/boot_core.h"
#define UKE_BOOT_PLATFORM_PROTOCOL_GUID \
  {0x89b770ba, 0x435e, 0x4308, {0xb5, 0x59, 0xaf, 0x4f, 0xb8, 0xe3, 0x51, 0x32}}
#define UKE_BOOT_PLATFORM_REVISION 1
#define UKE_MAX_INTERNAL_MEDIA 16
typedef struct UKE_BOOT_PLATFORM UKE_BOOT_PLATFORM;
struct UKE_BOOT_PLATFORM {
  UINT32 Revision;
  CHAR8 Profile[UKE_PROFILE_BYTES];
  /* Firmware-owned exact GPT GUID binding or NULL for unique-eligible policy. */
  CONST UINT8 *BoundEspGuid;
  /* TRUE only when saved entry context, original kernel/FDT, memory ownership
   * and quiescence are usable after ALL installed drivers, without an ESP. */
  BOOLEAN (EFIAPI *AndroidReady)(UKE_BOOT_PLATFORM *This);
  /* Returns only admitted internal whole UFS media, never USB or partitions.
   * Index >= count returns EFI_NOT_FOUND. All reads have hardware timeouts,
   * stable identity, supported geometry and no writes/repair/mount. */
  EFI_STATUS (EFIAPI *GetInternalMedia)(UKE_BOOT_PLATFORM *This, UINTN Index, UKE_DISK *Disk);
  /* Fresh complete firmware-owned path: exact media + GPT partition + the
   * passed validated installed file path. Caller releases the pool with FreePool. */
  EFI_STATUS (EFIAPI *ManagerDevicePath)(UKE_BOOT_PLATFORM *This, UINTN Media,
      CONST UKE_PARTITION *Esp, CONST CHAR8 *Path, EFI_DEVICE_PATH_PROTOCOL **DevicePath);
  /* Monotonic time; a backwards clock cancels discovery. */
  UINT64 (EFIAPI *Milliseconds)(UKE_BOOT_PLATFORM *This);
  /* Quiesce DMA/devices, leave EFI services safely, restore the required
   * EL/MMU/cache/interrupt/timer state and x0-x3, then enter original Android.
   * Must not return, reset into Aloha, depend on ESP, or use donor pstore. */
  EFI_STATUS (EFIAPI *ResumeAndroid)(UKE_BOOT_PLATFORM *This, UKE_STATUS Reason);
};
#endif
