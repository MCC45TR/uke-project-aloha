/* SPDX-License-Identifier: MIT */
#include <Uefi.h>
#include <Protocol/UkeBootPlatform.h>
#include "uke/boot_core.h"

typedef struct {
  UKE_BOOT_PLATFORM *platform;
  EFI_SYSTEM_TABLE *st;
  EFI_HANDLE parent;
  UKE_DISK disks[UKE_MAX_INTERNAL_MEDIA];
  UKE_DISK raw_disks[UKE_MAX_INTERNAL_MEDIA];
  UKE_GPT_RESULT gpts[UKE_MAX_INTERNAL_MEDIA];
  UKE_MEDIA media[UKE_MAX_INTERNAL_MEDIA];
  UINT8 *image;
  size_t image_bytes;
  UINT64 started, last;
  UINTN reads;
} DISPATCH;
typedef struct { DISPATCH *dispatch; UINTN media; } READ_CONTEXT;
static EFI_GUID platform_guid = UKE_BOOT_PLATFORM_PROTOCOL_GUID;
static int bounded_read(void *opaque, uint64_t offset, size_t bytes, void *out) {
  READ_CONTEXT *reader = (READ_CONTEXT *)opaque;
  DISPATCH *d = reader->dispatch;
  UKE_DISK *disk = &d->raw_disks[reader->media];
  UINT8 *destination = (UINT8 *)out;
  if (offset > disk->bytes || bytes > disk->bytes - offset) return 0;
  while (bytes) {
    size_t take = bytes > UKE_MAX_BLOCK ? UKE_MAX_BLOCK : bytes;
    UINT64 now = d->platform->Milliseconds(d->platform);
    if (now < d->last || now - d->started > 10000 || ++d->reads > 131072) return 0;
    d->last = now;
    if (!disk->read(disk->context, offset, take, destination)) return 0;
    now = d->platform->Milliseconds(d->platform);
    if (now < d->last || now - d->started > 10000) return 0;
    d->last = now; offset += take; bytes -= take; destination += take;
  }
  return 1;
}
static UKE_STATUS verify(void *opaque, size_t media, const UKE_PARTITION *p, UKE_BOOT_CONFIG *config) {
  DISPATCH *d = (DISPATCH *)opaque;
  UKE_FAT32 volume;
  UKE_STATUS status = uke_fat32_open(&d->disks[media], p, &volume);
  if (status != UKE_OK) return status;
  return uke_esp_manager_probe(&volume, d->platform->Profile, d->image, UKE_MAX_IMAGE,
    config, &d->image_bytes);
}
static int android_ready(void *opaque) {
  DISPATCH *d = (DISPATCH *)opaque;
  return d->platform->AndroidReady(d->platform);
}
static UKE_STATUS resume(void *opaque, UKE_STATUS reason) {
  DISPATCH *d = (DISPATCH *)opaque;
  (void)d->platform->ResumeAndroid(d->platform, reason);
  return UKE_BACKEND_UNAVAILABLE; // A returned EFI status is never a boot success.
}
static UKE_STATUS launch(void *opaque, const UKE_BOOT_DECISION *decision) {
  DISPATCH *d = (DISPATCH *)opaque;
  EFI_BOOT_SERVICES *bs = d->st->BootServices;
  EFI_DEVICE_PATH_PROTOCOL *path = NULL;
  EFI_HANDLE manager = NULL;
  EFI_STATUS status;
  UKE_BOOT_CONFIG fresh;
  UKE_STATUS checked = verify(d, decision->media, &decision->partition, &fresh);
  UINTN i;
  if (checked != UKE_OK) return checked;
  if (fresh.manager != decision->config.manager) return UKE_INVALID_CONFIG;
  for (i = 0; i < UKE_MANAGER_PATH_BYTES; ++i)
    if (fresh.path[i] != decision->config.path[i]) return UKE_INVALID_CONFIG;
  for (i = 0; i < 32; ++i) if (fresh.sha256[i] != decision->config.sha256[i]) return UKE_HASH_MISMATCH;
  status = d->platform->ManagerDevicePath(d->platform, decision->media, &decision->partition,
      uke_manager_path(&decision->config), &path);
  if (EFI_ERROR(status) || !path) { if (path) bs->FreePool(path); return UKE_BACKEND_UNAVAILABLE; }
  status = bs->LoadImage(FALSE, d->parent, path, d->image, d->image_bytes, &manager);
  bs->FreePool(path);
  if (EFI_ERROR(status) || !manager) {
    if (manager) bs->UnloadImage(manager);
    return UKE_INVALID_IMAGE; // Includes authentication rejection, never bypassed.
  }
  status = bs->StartImage(manager, NULL, NULL);
  (void)status;
  return UKE_MANAGER_RETURNED;
}
EFI_STATUS EFIAPI UkeBootDispatchEntry(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
  UKE_BOOT_PLATFORM *platform = NULL;
  DISPATCH *d = NULL;
  UKE_GPT_WORKSPACE *workspace = NULL;
  READ_CONTEXT readers[UKE_MAX_INTERNAL_MEDIA];
  UKE_DISCOVERY discovery;
  UKE_DISPATCH_BACKEND backend;
  UKE_BOOT_DECISION decision;
  EFI_BOOT_SERVICES *bs;
  EFI_STATUS status;
  UINTN count, i;
  if (!st || !st->BootServices) return EFI_INVALID_PARAMETER;
  bs = st->BootServices;
  status = bs->LocateProtocol(&platform_guid, NULL, (VOID **)&platform);
  if (EFI_ERROR(status) || !platform || platform->Revision != UKE_BOOT_PLATFORM_REVISION ||
      !platform->AndroidReady || !platform->ResumeAndroid || !platform->GetInternalMedia ||
      !platform->ManagerDevicePath || !platform->Milliseconds) return EFI_NOT_READY;
  for (i = 0; i < UKE_PROFILE_BYTES && platform->Profile[i]; ++i)
    if ((UINT8)platform->Profile[i] < 33 || (UINT8)platform->Profile[i] > 126) return EFI_INVALID_PARAMETER;
  if (!i || i == UKE_PROFILE_BYTES || !platform->AndroidReady(platform)) return EFI_NOT_READY;
  status = bs->AllocatePool(EfiBootServicesData, sizeof(*d), (VOID **)&d);
  if (EFI_ERROR(status)) { platform->ResumeAndroid(platform, UKE_IO_ERROR); return EFI_ABORTED; }
  for (i = 0; i < sizeof(*d); ++i) ((UINT8 *)d)[i] = 0;
  d->platform = platform; d->st = st; d->parent = image;
  status = bs->AllocatePool(EfiBootServicesData, sizeof(*workspace), (VOID **)&workspace);
  if (!EFI_ERROR(status)) status = bs->AllocatePool(EfiBootServicesData, UKE_MAX_IMAGE, (VOID **)&d->image);
  if (EFI_ERROR(status)) goto fallback;
  d->started = d->last = platform->Milliseconds(platform);
  for (count = 0; count < UKE_MAX_INTERNAL_MEDIA; ++count) {
    status = platform->GetInternalMedia(platform, count, &d->raw_disks[count]);
    if (status == EFI_NOT_FOUND) break;
    if (EFI_ERROR(status) || !d->raw_disks[count].read) goto fallback;
    readers[count].dispatch = d; readers[count].media = count;
    d->disks[count] = d->raw_disks[count];
    d->disks[count].context = &readers[count]; d->disks[count].read = bounded_read;
    d->media[count].admitted_internal = 1; d->media[count].gpt = &d->gpts[count];
    d->media[count].scan_status = uke_gpt_scan(&d->disks[count], workspace, &d->gpts[count]);
  }
  if (count == UKE_MAX_INTERNAL_MEDIA) {
    UKE_DISK extra;
    if (platform->GetInternalMedia(platform, count, &extra) != EFI_NOT_FOUND) goto fallback;
  }
  discovery = (UKE_DISCOVERY){d->media, count, platform->BoundEspGuid, d, verify};
  backend = (UKE_DISPATCH_BACKEND){d, android_ready, launch, resume};
  (void)uke_boot_dispatch(&discovery, &backend, &decision);
  goto cleanup;
fallback:
  platform->ResumeAndroid(platform, UKE_BACKEND_UNAVAILABLE);
cleanup:
  if (d->image) bs->FreePool(d->image);
  if (workspace) bs->FreePool(workspace);
  bs->FreePool(d);
  return EFI_ABORTED;
}
