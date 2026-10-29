/* SPDX-License-Identifier: MIT */
/* Hosted EFI-service mocks. The production source is included to exercise its
 * actual I/O guard and failure paths; no firmware or hardware is executed. */
#include "../src/platform/UkePlatformPkg/BootDispatcher/BootDispatcher.c"
extern VOID *malloc(UINTN bytes);
extern VOID free(VOID *pointer);
extern int printf(const char *format, ...);
extern void abort(void);
extern VOID *fopen(const char *path, const char *mode);
extern int fseek(VOID *stream, long offset, int origin);
extern long ftell(VOID *stream);
extern UINTN fread(VOID *out, UINTN size, UINTN count, VOID *stream);
extern int fclose(VOID *stream);
static UINTN allocations, allocation_calls, fail_allocation, media_calls, resumes, reads;
static BOOLEAN present = TRUE, ready = TRUE, media_error = FALSE, read_error = FALSE;
static UINT64 clock_ms;
static UKE_STATUS resume_reason;
static UKE_BOOT_PLATFORM provider;
static VOID *fixture;
static UINT64 fixture_bytes;
static const char *expected_path;
static EFI_DEVICE_PATH_PROTOCOL *path_seen;
static UINTN launch_mode, paths, loads, starts, unloads;
static void check(int passed, const char *message) {
  if (!passed) { printf("FAIL: %s\n", message); abort(); }
}
static BOOLEAN EFIAPI mock_ready(UKE_BOOT_PLATFORM *this) { (void)this; return ready; }
static UINT64 EFIAPI mock_clock(UKE_BOOT_PLATFORM *this) { (void)this; return clock_ms; }
static EFI_STATUS EFIAPI mock_resume(UKE_BOOT_PLATFORM *this, UKE_STATUS reason) {
  (void)this; ++resumes; resume_reason = reason; return EFI_ABORTED;
}
static int fixture_read(void *context, uint64_t offset, size_t bytes, void *out) {
  return offset <= 0x7fffffff && fseek(context, (long)offset, 0) == 0 &&
    fread(out, 1, bytes, context) == bytes;
}
static EFI_STATUS EFIAPI mock_media(UKE_BOOT_PLATFORM *this, UINTN index, UKE_DISK *disk) {
  (void)this; ++media_calls;
  if (fixture && !index) {
    *disk = (UKE_DISK){fixture, fixture_read, fixture_bytes, 512};
    return EFI_SUCCESS;
  }
  return media_error ? EFI_DEVICE_ERROR : EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI mock_allocate(EFI_MEMORY_TYPE type, UINTN bytes, VOID **out);
static EFI_STATUS EFIAPI mock_path(UKE_BOOT_PLATFORM *this, UINTN media,
    CONST UKE_PARTITION *esp, CONST CHAR8 *path, EFI_DEVICE_PATH_PROTOCOL **out) {
  UINTN i;
  (void)this; (void)esp; ++paths;
  check(fixture && media == 0, "only selected fixture media supplies the device path");
  for (i = 0; path[i] && expected_path[i] && path[i] == expected_path[i]; ++i) {}
  check(path[i] == expected_path[i], "actual installed path reaches the platform provider");
  if (launch_mode == 3) return EFI_UNSUPPORTED;
  if (EFI_ERROR(mock_allocate(EfiBootServicesData, sizeof(**out), (VOID **)out))) return EFI_OUT_OF_RESOURCES;
  path_seen = *out;
  return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_load(BOOLEAN boot, EFI_HANDLE parent, EFI_DEVICE_PATH_PROTOCOL *path,
    VOID *source, UINTN bytes, EFI_HANDLE *handle) {
  (void)parent; ++loads;
  check(!boot && path == path_seen && bytes > 64 && ((UINT8 *)source)[0] == 'M', "selected image supplied to LoadImage once");
  *handle = &provider;
  return launch_mode == 1 ? EFI_SECURITY_VIOLATION : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_start(EFI_HANDLE handle, UINTN *exit_size, CHAR16 **exit_data) {
  (void)exit_size; (void)exit_data; ++starts;
  check(handle == &provider && starts == 1, "only the accepted manager is started once");
  return launch_mode == 2 ? EFI_ABORTED : EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_unload(EFI_HANDLE handle) {
  check(handle == &provider, "unload only rejected LoadImage handle"); ++unloads; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_locate(EFI_GUID *guid, VOID *registration, VOID **out) {
  (void)guid; (void)registration; *out = present ? &provider : NULL;
  return present ? EFI_SUCCESS : EFI_NOT_FOUND;
}
static EFI_STATUS EFIAPI mock_allocate(EFI_MEMORY_TYPE type, UINTN bytes, VOID **out) {
  (void)type;
  if (++allocation_calls == fail_allocation) return EFI_OUT_OF_RESOURCES;
  *out = malloc(bytes);
  if (!*out) return EFI_OUT_OF_RESOURCES;
  ++allocations; return EFI_SUCCESS;
}
static EFI_STATUS EFIAPI mock_free(VOID *p) { free(p); --allocations; return EFI_SUCCESS; }
static int mock_read(void *context, uint64_t offset, size_t bytes, void *out) {
  UINTN i; (void)context; (void)offset; ++reads;
  check(bytes <= UKE_MAX_BLOCK, "underlying read is at most one 4 KiB chunk");
  if (read_error) return 0;
  for (i = 0; i < bytes; ++i) ((UINT8 *)out)[i] = 42;
  return 1;
}
int main(int argc, char **argv) {
  EFI_BOOT_SERVICES services = {0};
  EFI_SYSTEM_TABLE st = {0};
  UINTN mode, i;
  DISPATCH d = {0}; READ_CONTEXT reader = {&d, 0}; UINT8 buffer[8192];
  services.LocateProtocol = mock_locate; services.AllocatePool = mock_allocate;
  services.FreePool = mock_free; st.BootServices = &services;
  services.LoadImage = mock_load; services.StartImage = mock_start; services.UnloadImage = mock_unload;
  provider.Revision = UKE_BOOT_PLATFORM_REVISION;
  provider.Profile[0] = 'u'; provider.Profile[1] = 0;
  provider.AndroidReady = mock_ready; provider.ResumeAndroid = mock_resume;
  provider.GetInternalMedia = mock_media; provider.ManagerDevicePath = mock_path;
  provider.Milliseconds = mock_clock;
  for (mode = 0; mode < 7; ++mode) {
    EFI_STATUS result;
    present = mode != 0; ready = mode != 1; media_error = mode == 6;
    fail_allocation = mode >= 3 && mode <= 5 ? mode - 2 : 0;
    allocations = allocation_calls = media_calls = resumes = 0;
    result = UkeBootDispatchEntry(NULL, &st);
    check(allocations == 0, "all allocated pools released when backend returns");
    if (mode < 2) {
      check(result == EFI_NOT_READY && resumes == 0 && media_calls == 0 && allocation_calls == 0,
            "no discovery or allocation before platform/return admission");
    } else {
      check(result == EFI_ABORTED && resumes == 1, "one fallback and no false boot success");
      if (mode >= 3 && mode <= 5) check(media_calls == 0, "allocation failure precedes media discovery");
      if (mode == 3) check(resume_reason == UKE_IO_ERROR, "first allocation failure classified");
    }
  }
  ready = TRUE; d.platform = &provider;
  d.raw_disks[0] = (UKE_DISK){NULL, mock_read, 65536, 512};
  reads = 0;
  check(bounded_read(&reader, 0, sizeof(buffer), buffer) && reads == 2,
        "larger FAT cluster reads split instead of rejecting valid media");
  for (i = 0; i < sizeof(buffer); ++i) check(buffer[i] == 42, "every chunk copied");
  check(!bounded_read(&reader, 65536, 1, buffer) && reads == 2, "bounds rejected before I/O");
  clock_ms = 10001;
  check(!bounded_read(&reader, 0, 1, buffer) && reads == 2, "deadline rejected before I/O");
  clock_ms = 0; d.last = 1;
  check(!bounded_read(&reader, 0, 1, buffer) && reads == 2, "backwards clock rejected");
  d.last = 0; d.reads = 131072;
  check(!bounded_read(&reader, 0, 1, buffer) && reads == 2, "global read budget enforced");
  d.reads = 0; read_error = TRUE;
  check(!bounded_read(&reader, 0, 1, buffer) && reads == 3, "hardware read failure propagated");
  printf("EFI dispatcher host mocks: 7 lifecycle + 6 bounded-I/O scenarios passed\n");
  if (argc == 3) {
    long length;
    fixture = fopen(argv[1], "rb"); expected_path = argv[2];
    check(fixture && !fseek(fixture, 0, 2), "read-only GPT/FAT32 fixture opened");
    length = ftell(fixture); check(length > 0, "fixture size read"); fixture_bytes = (UINT64)length;
    present = ready = TRUE; media_error = FALSE; clock_ms = fail_allocation = 0;
    for (launch_mode = 0; launch_mode < 4; ++launch_mode) {
      allocations = allocation_calls = resumes = paths = loads = starts = unloads = 0;
      check(UkeBootDispatchEntry(NULL, &st) == EFI_ABORTED, "returning manager always requests Android fallback");
      check(!allocations && resumes == 1 && paths == 1, "one Android request and all pools released");
      check(loads == (launch_mode == 3 ? 0 : 1), "device-path failure never loads an image");
      check(starts == (launch_mode == 0 || launch_mode == 2 ? 1 : 0), "authentication rejection never starts manager");
      check(unloads == (launch_mode == 1 ? 1 : 0), "StartImage return never double-unloads application");
      check(resume_reason == (launch_mode == 1 ? UKE_INVALID_IMAGE : launch_mode == 3 ?
        UKE_BACKEND_UNAVAILABLE : UKE_MANAGER_RETURNED), "exact launch failure reaches Android backend");
    }
    check(!fclose(fixture), "fixture closed");
    printf("EFI dispatcher installed-path host mocks: 4 launch/fallback scenarios passed\n");
  } else check(argc == 1, "optional arguments are fixture and installed path");
  return 0;
}
