/* SPDX-License-Identifier: MIT */
#include <Uefi.h>
#include <Guid/Fdt.h>
#include <Protocol/GraphicsOutput.h>
#include "uke/linux_handoff.h"
static EFI_GUID fdt_guid = FDT_TABLE_GUID;
static EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
static void line(EFI_SYSTEM_TABLE *st, const char *s) {
  CHAR16 text[160];
  UINTN n = 0;
  if (!st->ConOut) return;
  while (s[n] && n < 156) { text[n] = (CHAR16)(UINT8)s[n]; ++n; }
  text[n++] = '\r'; text[n++] = '\n'; text[n] = 0;
  st->ConOut->OutputString(st->ConOut, text);
}
static void number(EFI_SYSTEM_TABLE *st, const char *label, UINT64 value) {
  char text[120], digits[20];
  UINTN used = 0, count = 0;
  while (*label && used < 90) text[used++] = *label++;
  do { digits[count++] = (char)('0' + value % 10); value /= 10; } while (value);
  while (count) text[used++] = digits[--count];
  text[used] = 0; line(st, text);
}
static int same_guid(const EFI_GUID *a, const EFI_GUID *b) {
  const UINT8 *x = (const UINT8 *)a, *y = (const UINT8 *)b;
  UINTN i;
  for (i = 0; i < sizeof(*a); ++i) if (x[i] != y[i]) return 0;
  return 1;
}
EFI_STATUS EFIAPI UkeLinuxHandoffEntry(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
  EFI_BOOT_SERVICES *bs;
  EFI_STATUS status;
  UINTN bytes = 0, stride = 0, key = 0, allocated, attempt, i;
  UINT32 version = 0;
  VOID *map = NULL;
  UKE_MEMORY_INVENTORY inventory;
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop = NULL;
  int fdt_present = 0, map_valid = 0;
  (void)image;
  if (!st || !st->BootServices) return EFI_INVALID_PARAMETER;
  bs = st->BootServices;
  line(st, "Uke Linux handoff diagnostic: no OS launch, storage or variable writes");
  // Boot-service allocations can grow the map. Retry a bounded number of times.
  status = bs->GetMemoryMap(&bytes, NULL, &key, &stride, &version);
  if (status != EFI_BUFFER_TOO_SMALL) return EFI_UNSUPPORTED;
  for (attempt = 0; attempt < 4; ++attempt) {
    if (stride < 40 || stride > 256 || bytes > 512 * 1024) break;
    allocated = bytes + 16 * stride;
    status = bs->AllocatePool(EfiBootServicesData, allocated, &map);
    if (EFI_ERROR(status)) return status;
    bytes = allocated;
    status = bs->GetMemoryMap(&bytes, map, &key, &stride, &version);
    if (status == EFI_BUFFER_TOO_SMALL) { bs->FreePool(map); map = NULL; continue; }
    if (!EFI_ERROR(status)) map_valid = uke_memory_map_check(map, bytes, stride, version, &inventory);
    if (map_valid) {
      number(st, "EFI descriptors: ", inventory.descriptor_count);
      number(st, "Conventional RAM bytes in this diagnostic snapshot: ", inventory.conventional_bytes);
      number(st, "Runtime descriptors: ", inventory.runtime_descriptors);
    }
    bs->FreePool(map); map = NULL; break;
  }
  line(st, map_valid ? "EFI map structure: PASS; profile ownership unverified" : "EFI map structure: FAIL");
  if (st->NumberOfTableEntries <= 256 && st->ConfigurationTable) {
    for (i = 0; i < st->NumberOfTableEntries; ++i)
      if (same_guid(&st->ConfigurationTable[i].VendorGuid, &fdt_guid) &&
          st->ConfigurationTable[i].VendorTable) ++fdt_present;
  }
  line(st, fdt_present == 1 ? "One firmware FDT table present; contents/board unverified" : "Firmware FDT table absent or ambiguous");
  status = bs->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
  if (!EFI_ERROR(status) && gop && gop->Mode && gop->Mode->Info) {
    number(st, "GOP width: ", gop->Mode->Info->HorizontalResolution);
    number(st, "GOP height: ", gop->Mode->Info->VerticalResolution);
    number(st, "GOP pixels per scanline: ", gop->Mode->Info->PixelsPerScanLine);
    number(st, "GOP pixel format enum: ", gop->Mode->Info->PixelFormat);
    number(st, "GOP framebuffer bytes: ", gop->Mode->FrameBufferSize);
  } else line(st, "GOP unavailable: no framebuffer geometry assumed");
  line(st, "Linux EFI stub owns final map transfer and ExitBootServices.");
  line(st, "This snapshot is not the final kernel handoff or a Uke boot result.");
  return map_valid ? EFI_SUCCESS : EFI_UNSUPPORTED;
}
