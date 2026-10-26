/* SPDX-License-Identifier: MIT */
#include <UkeUfsReadOnly.h>
#include <Protocol/DevicePath.h>
#include <Protocol/ScsiPassThruExt.h>
static UINT64 be(CONST UINT8 *p, UINTN bytes) {
  UINT64 v = 0; UINTN i;
  for (i = 0; i < bytes; ++i) v = (v << 8) | p[i];
  return v;
}
static void put(UINT8 *p, UINT64 v, UINTN bytes) {
  UINTN i;
  for (i = 0; i < bytes; ++i) p[i] = (UINT8)(v >> (8 * (bytes - i - 1)));
}
static EFI_STATUS command(UKE_UFS_READER *r, UINT8 *cdb, UINT32 bytes) {
  EFI_SCSI_IO_SCSI_REQUEST_PACKET packet = {0};
  UINT8 sense[96] = {0};
  EFI_STATUS status;
  packet.Timeout = 5000000; // 500 ms, 100 ns units; never wait indefinitely.
  packet.InDataBuffer = r->bounce; packet.InTransferLength = bytes;
  packet.Cdb = cdb; packet.CdbLength = 16;
  packet.DataDirection = EFI_SCSI_IO_DATA_DIRECTION_READ;
  packet.SenseData = sense; packet.SenseDataLength = sizeof(sense);
  status = r->scsi->ExecuteScsiCommand(r->scsi, &packet, NULL);
  if (EFI_ERROR(status)) return status;
  if (packet.HostAdapterStatus != EFI_SCSI_IO_STATUS_HOST_ADAPTER_OK ||
      packet.TargetStatus != EFI_SCSI_IO_STATUS_TARGET_GOOD || packet.InTransferLength != bytes)
    return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static int path_identity(CONST UINT8 *p, UINTN bytes, UINT8 *pun, UINT8 *lun) {
  UINTN offset = 0, nodes = 0; BOOLEAN ufs = FALSE;
  while (offset < bytes) {
    UINTN length;
    if (++nodes > 64 || bytes - offset < 4) return 0;
    length = p[offset + 2] | ((UINTN)p[offset + 3] << 8);
    if (length < 4 || length > bytes - offset) return 0;
    if (p[offset] == END_DEVICE_PATH_TYPE)
      return p[offset + 1] == END_ENTIRE_DEVICE_PATH_SUBTYPE && length == 4 &&
          offset + length == bytes && ufs;
    if (p[offset] == MESSAGING_DEVICE_PATH) {
      /* No USB/SCSI bridge, multi-instance, media/partition or transport node. */
      if (p[offset + 1] != MSG_UFS_DP || length != sizeof(UFS_DEVICE_PATH) || ufs) return 0;
      *pun = p[offset + 4]; *lun = p[offset + 5]; ufs = TRUE;
      if (*pun != 0 || *lun > 7) return 0; // Mu native UFS uses PUN zero; no well-known/RPMB units.
    } else if (ufs || (p[offset] != HARDWARE_DEVICE_PATH && p[offset] != ACPI_DEVICE_PATH)) return 0;
    offset += length;
  }
  return 0;
}
void UkeUfsClose(UKE_UFS_READER *r) {
  if (r) *r = (UKE_UFS_READER){0}; // No reset, writes, unmap or controller mutation.
}
EFI_STATUS UkeUfsOpenReadOnly(UKE_UFS_READER *r, EFI_SCSI_IO_PROTOCOL *scsi,
    CONST VOID *path, UINTN path_bytes, CONST UINT8 approved[32], VOID *bounce, UINTN bounce_bytes) {
  UINT8 hash[32], pun = 0, lun = 0, type = 0, cdb[16] = {0};
  UINT8 target_storage[TARGET_MAX_BYTES] = {0}, *target = target_storage;
  UINTN i, align;
  EFI_STATUS status;
  UINT64 last;
  UINT64 reported_lun = 0;
  UINT32 block;
  if (!r) return EFI_INVALID_PARAMETER;
  UkeUfsClose(r);
  if (!scsi || !scsi->GetDeviceType || !scsi->GetDeviceLocation || !scsi->ExecuteScsiCommand || !path || !approved ||
      path_bytes < 10 || path_bytes > 4096 || !bounce || bounce_bytes < 4096 ||
      !path_identity((CONST UINT8 *)path, path_bytes, &pun, &lun)) return EFI_INVALID_PARAMETER;
  align = scsi->IoAlign > 1 ? scsi->IoAlign : 1;
  if (align > 4096 || (align & (align - 1)) || ((UINTN)bounce & (align - 1))) return EFI_UNSUPPORTED;
  uke_sha256(path, (size_t)path_bytes, hash);
  for (i = 0; i < 32; ++i) if (hash[i] != approved[i]) return EFI_SECURITY_VIOLATION;
  status = scsi->GetDeviceType(scsi, &type);
  if (EFI_ERROR(status) || type != EFI_SCSI_IO_TYPE_DISK) return EFI_UNSUPPORTED;
  status = scsi->GetDeviceLocation(scsi, &target, &reported_lun);
  /* Mu UFS maps common LU n to the second byte of the SCSI UINT64 LUN;
   * checking just its low byte would accidentally admit another unit. */
  if (EFI_ERROR(status) || target != target_storage || reported_lun != ((UINT64)lun << 8))
    return EFI_SECURITY_VIOLATION;
  for (i = 0; i < TARGET_MAX_BYTES; ++i) if (target_storage[i]) return EFI_SECURITY_VIOLATION;
  r->scsi = scsi; r->bounce = bounce; r->pun = pun; r->lun = lun;
  cdb[0] = 0x9e; cdb[1] = 0x10; put(cdb + 10, 32, 4); // READ CAPACITY (16).
  status = command(r, cdb, 32);
  if (EFI_ERROR(status)) { UkeUfsClose(r); return status; }
  last = be(r->bounce, 8); block = (UINT32)be(r->bounce + 8, 4);
  if (block < 512 || block > 4096 || (block & (block - 1)) || last >= UINT64_MAX / block) {
    UkeUfsClose(r); return EFI_UNSUPPORTED;
  }
  r->last_lba = last; r->block_size = block; r->ready = TRUE; return EFI_SUCCESS;
}
int UkeUfsRead(void *opaque, uint64_t offset, size_t bytes, void *out) {
  UKE_UFS_READER *r = (UKE_UFS_READER *)opaque;
  UINT8 *destination = out;
  UINT64 capacity;
  if (!r || !r->ready || !r->scsi || !r->bounce || !out || !bytes || bytes > 4096) return 0;
  capacity = (r->last_lba + 1) * r->block_size;
  if (offset > capacity || bytes > capacity - offset) return 0;
  while (bytes) {
    UINT8 cdb[16] = {0};
    size_t within = (size_t)(offset % r->block_size), take = r->block_size - within, i;
    cdb[0] = 0x88; put(cdb + 2, offset / r->block_size, 8); put(cdb + 10, 1, 4); // READ (16), one block.
    if (EFI_ERROR(command(r, cdb, r->block_size))) { r->ready = FALSE; return 0; }
    if (take > bytes) take = bytes;
    for (i = 0; i < take; ++i) destination[i] = r->bounce[within + i];
    bytes -= take; offset += take; destination += take;
  }
  return 1;
}
