/* SPDX-License-Identifier: MIT */
/* Generic EFI emulation only; no Uke protocol or Android backend is installed. */
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

EFI_STATUS EFIAPI MuDispatchSmokeEntry(EFI_HANDLE image, EFI_SYSTEM_TABLE *st) {
  EFI_LOADED_IMAGE_PROTOCOL *loaded = NULL;
  EFI_DEVICE_PATH_PROTOCOL *path = NULL;
  EFI_HANDLE child = NULL;
  EFI_STATUS status;
  BOOLEAN passed = FALSE;
  if (!st || !st->BootServices || !st->RuntimeServices || !st->ConOut)
    return EFI_INVALID_PARAMETER;
  status = st->BootServices->HandleProtocol(image, &gEfiLoadedImageProtocolGuid, (VOID **)&loaded);
  if (!EFI_ERROR(status) && loaded) {
    path = FileDevicePath(loaded->DeviceHandle, L"\\UkeBootDispatcher.efi");
    if (path) {
      status = st->BootServices->LoadImage(FALSE, image, path, NULL, 0, &child);
      FreePool(path);
      if (!EFI_ERROR(status)) {
        status = st->BootServices->StartImage(child, NULL, NULL);
        passed = status == EFI_NOT_READY;
      }
    }
  }
  st->ConOut->OutputString(st->ConOut, passed
    ? L"UKE-MU-QEMU: missing platform safely refused; actual EFI StartImage passed\r\n"
    : L"UKE-MU-QEMU: FAIL\r\n");
  st->RuntimeServices->ResetSystem(EfiResetShutdown, passed ? EFI_SUCCESS : EFI_ABORTED, 0, NULL);
  return EFI_ABORTED;
}
