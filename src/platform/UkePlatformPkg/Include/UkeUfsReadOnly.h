/* SPDX-License-Identifier: MIT */
#ifndef UKE_UFS_READ_ONLY_H
#define UKE_UFS_READ_ONLY_H
#include <Uefi.h>
#include <Protocol/ScsiIo.h>
#include "uke/boot_core.h"
typedef struct {
  EFI_SCSI_IO_PROTOCOL *scsi;
  UINT8 *bounce;
  UINT32 block_size;
  UINT64 last_lba;
  UINT8 pun, lun;
  BOOLEAN ready;
} UKE_UFS_READER;
/* Only an exact reviewed complete native UFS device path is admitted. This is
 * an adapter for a real preexisting EFI UFS/SCSI driver, not PHY initialization.
 * Caller obtains path and SCSI protocol from the SAME EFI child handle and
 * supplies an aligned 4096-byte bounce buffer with protected lifetime. The
 * reported SCSI target/LUN is cross-checked before any data command. */
EFI_STATUS UkeUfsOpenReadOnly(UKE_UFS_READER *reader, EFI_SCSI_IO_PROTOCOL *scsi,
    CONST VOID *device_path, UINTN path_bytes, CONST UINT8 admitted_path_sha256[32],
    VOID *bounce, UINTN bounce_bytes);
int UkeUfsRead(void *context, uint64_t offset, size_t bytes, void *out);
void UkeUfsClose(UKE_UFS_READER *reader);
#endif
