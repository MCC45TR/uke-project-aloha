/* SPDX-License-Identifier: MIT */
#include <UkeAndroidReturn.h>
#define UKE_MAX_MAP_BYTES (2u * 1024u * 1024u)
#define UKE_MAP_SLACK_DESCRIPTORS 16u
static int valid_map(UKE_ANDROID_RETURN *r) {
  return r->descriptor_version == EFI_MEMORY_DESCRIPTOR_VERSION &&
      r->descriptor_bytes >= sizeof(EFI_MEMORY_DESCRIPTOR) && r->descriptor_bytes <= 256 &&
      !(r->descriptor_bytes % 8) && r->map_bytes && r->map_bytes <= r->capacity &&
      !(r->map_bytes % r->descriptor_bytes);
}
static EFI_STATUS get_map(UKE_ANDROID_RETURN *r) {
  EFI_STATUS status;
  r->map_bytes = r->capacity;
  status = r->services->GetMemoryMap(&r->map_bytes, r->map, &r->key,
      &r->descriptor_bytes, &r->descriptor_version);
  if (!EFI_ERROR(status) && !valid_map(r)) return EFI_COMPROMISED_DATA;
  return status;
}
int UkeAndroidReturnReady(void *context) {
  UKE_ANDROID_RETURN *r = context;
  return r && !r->poisoned && !r->exit_attempted && !r->exited && r->image &&
      r->services && r->services->GetMemoryMap && r->services->AllocatePool &&
      r->services->FreePool && r->services->ExitBootServices &&
      r->RestoreReady && r->RestoreAndJump && r->RestoreReady(r->board_context);
}
EFI_STATUS UkeAndroidReturnRelease(UKE_ANDROID_RETURN *r) {
  EFI_STATUS status = EFI_SUCCESS;
  if (!r || r->exit_attempted || r->exited || !r->services || !r->services->FreePool)
    return EFI_ACCESS_DENIED;
  if (r->map) status = r->services->FreePool(r->map);
  if (!EFI_ERROR(status)) {
    r->map = NULL; r->capacity = 0; r->map_bytes = 0; r->prepared = FALSE;
  } else r->poisoned = TRUE;
  return status;
}
UKE_STATUS UkeAndroidReturnPrepare(void *context) {
  UKE_ANDROID_RETURN *r = context;
  EFI_STATUS status;
  UINTN required = 0, key = 0, descriptor_bytes = 0, capacity;
  UINT32 version = 0;
  if (!UkeAndroidReturnReady(r) || r->prepared || r->map) return UKE_BACKEND_UNAVAILABLE;
  status = r->services->GetMemoryMap(&required, NULL, &key, &descriptor_bytes, &version);
  if (status != EFI_BUFFER_TOO_SMALL || !required || required > UKE_MAX_MAP_BYTES ||
      descriptor_bytes < sizeof(EFI_MEMORY_DESCRIPTOR) || descriptor_bytes > 256 ||
      descriptor_bytes % 8 || version != EFI_MEMORY_DESCRIPTOR_VERSION ||
      required > UKE_MAX_MAP_BYTES - descriptor_bytes * UKE_MAP_SLACK_DESCRIPTORS)
    return UKE_BACKEND_UNAVAILABLE;
  capacity = required + descriptor_bytes * UKE_MAP_SLACK_DESCRIPTORS;
  status = r->services->AllocatePool(EfiLoaderData, capacity, (VOID **)&r->map);
  if (EFI_ERROR(status) || !r->map) { r->map = NULL; return UKE_BACKEND_UNAVAILABLE; }
  r->capacity = capacity;
  if (EFI_ERROR(get_map(r))) {
    (void)UkeAndroidReturnRelease(r); return UKE_BACKEND_UNAVAILABLE;
  }
  r->prepared = TRUE; return UKE_OK;
}
UKE_STATUS UkeAndroidExitServices(void *context) {
  UKE_ANDROID_RETURN *r = context;
  UINTN attempt;
  if (!r || !r->prepared || !r->map || !UkeAndroidReturnReady(r)) return UKE_BACKEND_UNAVAILABLE;
  /* No allocation, logging, FreePool or device work between map and exit.
   * A stale map permits one GetMemoryMap/ExitBootServices-only retry. */
  for (attempt = 0; attempt < 2; ++attempt) {
    EFI_STATUS status = get_map(r);
    if (EFI_ERROR(status)) { r->poisoned = TRUE; return UKE_BACKEND_UNAVAILABLE; }
    r->exit_attempted = TRUE;
    status = r->services->ExitBootServices(r->image, r->key);
    if (!EFI_ERROR(status)) { r->exited = TRUE; return UKE_OK; }
    if (status != EFI_INVALID_PARAMETER) break;
  }
  r->poisoned = TRUE; return UKE_BACKEND_UNAVAILABLE;
}
UKE_STATUS UkeAndroidRestoreAndJump(void *context) {
  UKE_ANDROID_RETURN *r = context;
  if (!r || !r->exited || r->poisoned || !r->RestoreAndJump) return UKE_BACKEND_UNAVAILABLE;
  r->poisoned = TRUE; // A returned architecture jump must never be retried.
  (void)r->RestoreAndJump(r->board_context);
  return UKE_BACKEND_UNAVAILABLE;
}
