/* SPDX-License-Identifier: MIT */
#include <Uefi.h>
#include <UkeUfsReadOnly.h>
extern int printf(const char *, ...);
extern void abort(void);
static unsigned checks, calls;
static EFI_STATUS injected = EFI_SUCCESS;
static UINT8 target_status, host_status;
static BOOLEAN short_read;
static UINT64 last_lba = 1023;
static UINT32 block_size = 512;
static unsigned location_mode;
static _Alignas(4096) UINT8 bounce[4096];
static void check(int value, const char *message) {
  ++checks; if (!value) { printf("FAIL: %s\n", message); abort(); }
}
static EFI_STATUS EFIAPI type(EFI_SCSI_IO_PROTOCOL *scsi, UINT8 *out) {
  (void)scsi; *out = EFI_SCSI_IO_TYPE_DISK; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI location(EFI_SCSI_IO_PROTOCOL *scsi, UINT8 **target, UINT64 *lun) {
  UINTN i; (void)scsi;
  for (i = 0; i < 16; ++i) (*target)[i] = 0;
  *lun = location_mode == 1 ? 2u << 8 : location_mode == 2 ? 1u << 20 :
      location_mode == 3 ? 0xc100u : 1u << 8;
  if (location_mode == 4) (*target)[15] = 1;
  return location_mode == 5 ? EFI_DEVICE_ERROR : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI execute(EFI_SCSI_IO_PROTOCOL *scsi,
    EFI_SCSI_IO_SCSI_REQUEST_PACKET *p, EFI_EVENT event) {
  UINT8 *cdb = p->Cdb, *data = p->InDataBuffer; UINTN i;
  (void)scsi; ++calls;
  check(!event && p->Timeout == 5000000 && !p->OutDataBuffer && p->OutTransferLength == 0 &&
      p->DataDirection == EFI_SCSI_IO_DATA_DIRECTION_READ && p->CdbLength == 16, "bounded synchronous read only");
  check(cdb[0] == 0x9e || cdb[0] == 0x88, "only capacity/read commands are issued");
  if (EFI_ERROR(injected)) return injected;
  if (cdb[0] == 0x9e) {
    check(cdb[1] == 0x10 && cdb[13] == 32 && p->InTransferLength == 32, "READ CAPACITY 16 layout");
    for (i = 0; i < 32; ++i) data[i] = 0;
    for (i = 0; i < 8; ++i) data[i] = (UINT8)(last_lba >> (8 * (7 - i)));
    for (i = 0; i < 4; ++i) data[8 + i] = (UINT8)(block_size >> (8 * (3 - i)));
  } else {
    check(cdb[13] == 1 && p->InTransferLength == block_size, "one-block READ 16");
    for (i = 0; i < p->InTransferLength; ++i) data[i] = (UINT8)(i % 251);
  }
  p->HostAdapterStatus = host_status; p->TargetStatus = target_status;
  if (short_read) --p->InTransferLength;
  return EFI_SUCCESS;
}
int main(void) {
  EFI_SCSI_IO_PROTOCOL scsi = {0};
  UINT8 path[] = {1, 4, 20, 0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 3,0x19,6,0,0,1, 0x7f,0xff,4,0};
  UINT8 hash[32], out[4096], bad[sizeof(path)]; UINTN i;
  UKE_UFS_READER reader;
  scsi.GetDeviceType = type; scsi.GetDeviceLocation = location;
  scsi.ExecuteScsiCommand = execute; scsi.IoAlign = 4096;
  uke_sha256(path, sizeof(path), hash);
  check(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce, sizeof(bounce)) == EFI_SUCCESS, "approved native UFS path opens");
  check(reader.ready && reader.block_size == 512 && reader.last_lba == 1023 && reader.lun == 1, "capacity and identity decoded");
  calls = 0;
  check(UkeUfsRead(&reader, 500, 32, out) && calls == 2, "unaligned read across two blocks");
  for (i = 0; i < 12; ++i) check(out[i] == (UINT8)((500 + i) % 251), "first block tail copied");
  for (i = 12; i < 32; ++i) check(out[i] == (UINT8)((i - 12) % 251), "second block head copied");
  check(!UkeUfsRead(&reader, 512 * 1024, 1, out) && calls == 2, "out-of-range before SCSI");
  short_read = TRUE;
  check(!UkeUfsRead(&reader, 0, 1, out) && !reader.ready, "short read invalidates reader");
  short_read = FALSE;
  for (i = 0; i < 8; ++i) {
    UINTN j;
    for (j = 0; j < sizeof(path); ++j) bad[j] = path[j];
    if (i == 0) bad[21] = 5; // USB transport.
    if (i == 1) bad[25] = 0x80; // well-known/RPMB unit.
    if (i == 2) bad[27] = 1; // multi-instance end.
    if (i == 3) bad[22] = 0xff; // bad length.
    if (i == 4) bad[20] = 4; // media/partition node.
    if (i == 5) bad[24] = 2; // hash not admitted.
    if (i == 6) bad[26] = 0; // no terminal end.
    if (i == 7) bad[24] = 1; // unsupported native PUN.
    calls = 0;
    check(EFI_ERROR(UkeUfsOpenReadOnly(&reader, &scsi, bad, sizeof(bad), hash, bounce, sizeof(bounce))) && !reader.ready && calls == 0,
        "unadmitted path rejected before any I/O");
  }
  for (location_mode = 1; location_mode <= 5; ++location_mode) {
    calls = 0;
    check(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce, sizeof(bounce)) == EFI_SECURITY_VIOLATION &&
        !reader.ready && !calls, "reported target/LUN mismatch rejected before data I/O");
  }
  location_mode = 0;
  scsi.GetDeviceLocation = NULL;
  check(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce, sizeof(bounce)) == EFI_INVALID_PARAMETER && !calls,
        "missing location backend rejected");
  scsi.GetDeviceLocation = location;
  for (i = 0; i < 5; ++i) {
    injected = i == 0 ? EFI_TIMEOUT : EFI_SUCCESS;
    host_status = i == 1 ? 0x7f : 0; target_status = i == 2 ? 2 : 0;
    short_read = i == 3; block_size = i == 4 ? 768 : 512;
    check(EFI_ERROR(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce, sizeof(bounce))) && !reader.ready,
          "transport/status/length/geometry failure rejects capacity");
  }
  block_size = 512; last_lba = UINT64_MAX; short_read = FALSE;
  check(EFI_ERROR(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce, sizeof(bounce))), "capacity overflow rejected");
  last_lba = 1023;
  check(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce + 1, sizeof(bounce) - 1) == EFI_INVALID_PARAMETER,
        "insufficient bounce rejected");
  check(UkeUfsOpenReadOnly(&reader, &scsi, path, sizeof(path), hash, bounce + 1, sizeof(bounce)) == EFI_UNSUPPORTED,
        "unaligned bounce rejected");
  UkeUfsClose(&reader);
  check(!reader.ready && !UkeUfsRead(&reader, 0, 1, out), "closed reader cannot access storage");
  printf("UFS read-only EFI mock checks=%u passed; no controller/PHY execution\n", checks);
  return 0;
}
