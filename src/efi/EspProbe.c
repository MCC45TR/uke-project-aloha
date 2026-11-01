/* SPDX-License-Identifier: MIT */
#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include "uke/boot_core.h"

static EFI_GUID block_guid = EFI_BLOCK_IO_PROTOCOL_GUID;
typedef struct {
  EFI_BLOCK_IO_PROTOCOL *io;
  UINT32 media_id;
  uint8_t *bounce;
  unsigned reads;
} READER;

/* This application inventories media; it never admits media for boot dispatch. */
static int read_disk(void *context, uint64_t offset, size_t bytes, void *output) {
  READER *r = (READER *)context;
  uint8_t *out = (uint8_t *)output;
  UINT32 block = r->io->Media->BlockSize;
  size_t done = 0;
  while (done < bytes) {
    size_t within = (size_t)(offset % block), take = block - within, i;
    EFI_STATUS status;
    if (++r->reads > 512 || r->io->Media->MediaId != r->media_id) return 0;
    status = r->io->ReadBlocks(r->io, r->media_id, offset / block, block, r->bounce);
    if (EFI_ERROR(status)) return 0;
    if (take > bytes - done) take = bytes - done;
    for (i = 0; i < take; ++i) out[done + i] = r->bounce[within + i];
    offset += take; done += take;
  }
  return 1;
}
static void line(EFI_SYSTEM_TABLE *st, const char *s) {
  CHAR16 text[128];
  size_t n = 0;
  if (!st->ConOut) return;
  while (s[n] && n < 124) { text[n] = (CHAR16)(uint8_t)s[n]; ++n; }
  text[n++] = '\r'; text[n++] = '\n'; text[n] = 0;
  st->ConOut->OutputString(st->ConOut, text);
}
EFI_STATUS EFIAPI UkeProbeEntry(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
  EFI_BOOT_SERVICES *bs;
  EFI_HANDLE *handles = NULL;
  UINTN count = 0, index;
  EFI_STATUS status;
  UKE_GPT_WORKSPACE *work = NULL;
  VOID *raw = NULL;
  (void)image;
  if (!st || !st->BootServices) return EFI_INVALID_PARAMETER;
  bs = st->BootServices;
  line(st, "Uke ESP probe 0.1: diagnostic, read-only; no OS dispatch");
  line(st, "No Uke platform or Android return backend is asserted.");
  status = bs->AllocatePool(EfiBootServicesData, sizeof(*work), (VOID **)&work);
  if (EFI_ERROR(status)) return status;
  status = bs->AllocatePool(EfiBootServicesData, UKE_MAX_BLOCK * 2, &raw);
  if (EFI_ERROR(status)) { bs->FreePool(work); return status; }
  status = bs->LocateHandleBuffer(ByProtocol, &block_guid, NULL, &count, &handles);
  if (!EFI_ERROR(status)) {
    if (count > 32) { line(st, "Too many media handles: refusing inventory"); count = 0; }
    for (index = 0; index < count; ++index) {
      EFI_BLOCK_IO_PROTOCOL *io = NULL;
      UKE_GPT_RESULT result;
      UKE_DISK disk;
      UKE_STATUS scanned;
      READER reader;
      UINTN align;
      status = bs->HandleProtocol(handles[index], &block_guid, (VOID **)&io);
      if (EFI_ERROR(status) || !io || !io->Media || !io->Media->MediaPresent || io->Media->LogicalPartition) continue;
      /* External media are diagnostic inputs only and are never boot candidates. */
      if (io->Media->BlockSize < 512 || io->Media->BlockSize > UKE_MAX_BLOCK ||
          (io->Media->BlockSize & (io->Media->BlockSize - 1)) ||
          io->Media->LastBlock >= UINT64_MAX / io->Media->BlockSize) {
        line(st, "Unsupported media geometry"); continue;
      }
      align = io->Media->IoAlign > 1 ? io->Media->IoAlign : 1;
      if (align > UKE_MAX_BLOCK || (align & (align - 1))) { line(st, "Unsupported I/O alignment"); continue; }
      reader.io = io; reader.media_id = io->Media->MediaId; reader.reads = 0;
      reader.bounce = (uint8_t *)(((UINTN)raw + align - 1) & ~(align - 1));
      disk.context = &reader; disk.read = read_disk; disk.block_size = io->Media->BlockSize;
      disk.bytes = (io->Media->LastBlock + 1) * io->Media->BlockSize;
      scanned = uke_gpt_scan(&disk, work, &result);
      line(st, uke_status_name(scanned));
      if (scanned == UKE_OK) line(st, result.count ? "ESP type found; filesystem/manager unverified" : "No ESP: Android fallback required by policy");
    }
  } else line(st, "No Block I/O: Android fallback required by policy");
  if (handles) bs->FreePool(handles);
  bs->FreePool(raw); bs->FreePool(work);
  line(st, "Returning to caller; caller is not assumed to be stock ABL.");
  return EFI_SUCCESS;
}
