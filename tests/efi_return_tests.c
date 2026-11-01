/* SPDX-License-Identifier: MIT */
#include <UkeAndroidReturn.h>
extern int printf(const char *, ...);
extern void abort(void);
extern void *malloc(UINTN);
extern void free(void *);
static unsigned checks, maps, allocations, frees, exits, jumps;
static unsigned mode;
static BOOLEAN board_ready = TRUE;
static void check(int value, const char *message) {
  ++checks; if (!value) { printf("FAIL: %s\n", message); abort(); }
}
static EFI_STATUS EFIAPI memory_map(UINTN *bytes, EFI_MEMORY_DESCRIPTOR *map,
    UINTN *key, UINTN *descriptor_bytes, UINT32 *version) {
  ++maps; *descriptor_bytes = mode == 4 ? 1 : sizeof(EFI_MEMORY_DESCRIPTOR);
  *version = mode == 5 ? 99 : EFI_MEMORY_DESCRIPTOR_VERSION;
  *key = maps;
  if (!map) { *bytes = 2 * sizeof(EFI_MEMORY_DESCRIPTOR); return EFI_BUFFER_TOO_SMALL; }
  if (mode == 7 && exits) { *bytes = 3 * 1024 * 1024; return EFI_BUFFER_TOO_SMALL; }
  if (mode == 6) { *bytes = sizeof(EFI_MEMORY_DESCRIPTOR) + 1; return EFI_SUCCESS; }
  if (mode == 8) return EFI_DEVICE_ERROR;
  check(*bytes >= 2 * sizeof(EFI_MEMORY_DESCRIPTOR), "map capacity includes slack");
  *bytes = 2 * sizeof(EFI_MEMORY_DESCRIPTOR); return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI allocate(EFI_MEMORY_TYPE type, UINTN bytes, VOID **out) {
  ++allocations;
  check(!exits && type == EfiLoaderData && bytes <= 2 * 1024 * 1024, "bounded allocation before first exit");
  if (mode == 9) return EFI_OUT_OF_RESOURCES;
  *out = malloc(bytes); return *out ? EFI_SUCCESS : EFI_OUT_OF_RESOURCES;
}
static EFI_STATUS EFIAPI release(VOID *allocation) {
  ++frees; check(!exits, "never FreePool after an exit attempt"); free(allocation); return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI exit_services(EFI_HANDLE image, UINTN key) {
  ++exits;
  check(image == (VOID *)1 && key == maps, "fresh key and exact image handle");
  if (mode == 1 && exits == 1) return EFI_INVALID_PARAMETER;
  if (mode == 2 || mode == 7) return EFI_INVALID_PARAMETER;
  if (mode == 3) return EFI_DEVICE_ERROR;
  return EFI_SUCCESS;
}
static BOOLEAN EFIAPI ready(VOID *context) { check(context == (VOID *)2, "exact board context"); return board_ready; }
static UKE_STATUS EFIAPI jump(VOID *context) {
  ++jumps; check(context == (VOID *)2 && exits, "restore after successful EFI exit"); return UKE_OK;
}
int main(void) {
  EFI_BOOT_SERVICES services = {0}; unsigned scenario;
  services.GetMemoryMap = memory_map; services.AllocatePool = allocate;
  services.FreePool = release; services.ExitBootServices = exit_services;
  for (scenario = 0; scenario < 10; ++scenario) {
    UKE_ANDROID_RETURN r = {0};
    mode = scenario; maps = allocations = frees = exits = jumps = 0;
    r.services = &services; r.image = (VOID *)1; r.board_context = (VOID *)2;
    r.RestoreReady = ready; r.RestoreAndJump = jump;
    check(UkeAndroidReturnReady(&r), "valid board contract gates ready");
    check(UkeAndroidRestoreAndJump(&r) == UKE_BACKEND_UNAVAILABLE && !jumps, "no jump before exit");
    if (mode >= 4 && mode != 7) {
      check(UkeAndroidReturnPrepare(&r) == UKE_BACKEND_UNAVAILABLE && !exits && !r.prepared,
            "bad map or allocation rejected before exit");
      check(!r.map && frees == allocations - (mode == 9), "failed prepare releases allocated map");
      continue;
    }
    check(UkeAndroidReturnPrepare(&r) == UKE_OK && r.prepared && !exits, "prepared without exit");
    check(UkeAndroidReturnPrepare(&r) == UKE_BACKEND_UNAVAILABLE && allocations == 1, "cannot prepare twice");
    if (mode == 0 || mode == 1) {
      check(UkeAndroidExitServices(&r) == UKE_OK && r.exited, "successful EFI exit");
      check(exits == (mode == 1 ? 2u : 1u), "single bounded stale-map retry");
      check(UkeAndroidRestoreAndJump(&r) == UKE_BACKEND_UNAVAILABLE && jumps == 1, "returned jump never reports Android success");
      check(UkeAndroidRestoreAndJump(&r) == UKE_BACKEND_UNAVAILABLE && jumps == 1, "no returned jump retry");
    } else {
      check(UkeAndroidExitServices(&r) == UKE_BACKEND_UNAVAILABLE && !r.exited && r.poisoned, "failed exit blocks restore");
      check(exits == (mode == 2 ? 2u : 1u), "retry limited to stale map");
      check(UkeAndroidRestoreAndJump(&r) == UKE_BACKEND_UNAVAILABLE && !jumps, "failed exit cannot jump");
    }
    check(UkeAndroidReturnRelease(&r) == EFI_ACCESS_DENIED && !frees, "services not reused after exit attempt");
    check(UkeAndroidExitServices(&r) == UKE_BACKEND_UNAVAILABLE, "no transaction retry");
    free(r.map); // Test harness owns mock allocations; this is not an EFI call.
  }
  {
    UKE_ANDROID_RETURN r = {0};
    r.services = &services; r.image = (VOID *)1; r.board_context = (VOID *)2;
    r.RestoreReady = ready; r.RestoreAndJump = jump;
    mode = 0; maps = allocations = frees = exits = jumps = 0;
    board_ready = FALSE;
    check(!UkeAndroidReturnReady(&r) && UkeAndroidReturnPrepare(&r) == UKE_BACKEND_UNAVAILABLE && !maps,
          "unavailable real restoration never touches EFI");
    board_ready = TRUE;
    check(UkeAndroidReturnPrepare(&r) == UKE_OK && UkeAndroidReturnRelease(&r) == EFI_SUCCESS && frees == 1 && !r.prepared,
          "prepared transaction can be cancelled before exit");
  }
  printf("EFI Android return mock checks=%u passed; no physical CPU restoration\n", checks);
  return 0;
}
