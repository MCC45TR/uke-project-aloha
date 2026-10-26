/* SPDX-License-Identifier: MIT */
#ifndef UKE_BOOT_CORE_H
#define UKE_BOOT_CORE_H
#ifdef UKE_EDK2
#include <Base.h>
typedef UINT8 uint8_t;
typedef UINT16 uint16_t;
typedef UINT32 uint32_t;
typedef UINT64 uint64_t;
typedef UINTN size_t;
#define UINT64_MAX MAX_UINT64
#else
#include <stddef.h>
#include <stdint.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif

#define UKE_MAX_PARTITIONS 128u
#define UKE_MAX_ESPS 16u
#define UKE_MAX_BLOCK 4096u
#define UKE_MAX_CONFIG 512u
#define UKE_PROFILE_BYTES 96u
#define UKE_MANAGER_PATH_BYTES 128u
#define UKE_MAX_IMAGE (32u * 1024u * 1024u)

typedef enum {
  UKE_OK = 0, UKE_INVALID_ARGUMENT, UKE_IO_ERROR, UKE_UNSUPPORTED_GEOMETRY,
  UKE_INVALID_MBR, UKE_INVALID_GPT, UKE_GPT_MISMATCH, UKE_PARTITION_OVERLAP,
  UKE_TOO_MANY_ESPS, UKE_NO_ESP, UKE_AMBIGUOUS_ESP, UKE_BOUND_ESP_MISSING,
  UKE_INVALID_CONFIG, UKE_WRONG_PROFILE, UKE_INVALID_IMAGE, UKE_HASH_MISMATCH,
  UKE_BACKEND_UNAVAILABLE, UKE_EXTERNAL_MEDIA, UKE_MANAGER_RETURNED,
  UKE_INVALID_FILESYSTEM, UKE_FILE_MISSING, UKE_FILE_TOO_LARGE, UKE_DIRECTORY_AMBIGUOUS,
  UKE_AMBIGUOUS_MANAGER
} UKE_STATUS;

/* Read-only, synchronous callback; backend must enforce deadlines and media identity. */
typedef int (*UKE_READ)(void *context, uint64_t offset, size_t bytes, void *output);
typedef struct {
  void *context;
  UKE_READ read;
  uint64_t bytes;
  uint32_t block_size;
} UKE_DISK;
typedef struct {
  uint8_t guid[16]; /* GPT on-disk GUID byte order, never a disk enumeration index. */
  uint64_t first_lba;
  uint64_t last_lba;
} UKE_PARTITION;
typedef struct {
  uint8_t disk_guid[16];
  uint32_t count;
  UKE_PARTITION esps[UKE_MAX_ESPS];
  uint32_t named_esp_mask; /* GPT UTF-16 name exactly ESP, case insensitive. */
} UKE_GPT_RESULT;
typedef struct {
  uint8_t block[UKE_MAX_BLOCK];
  UKE_PARTITION used[UKE_MAX_PARTITIONS];
} UKE_GPT_WORKSPACE;

/* Both GPT copies and arrays must agree. Count is zero on failure; no repair. */
UKE_STATUS uke_gpt_scan(const UKE_DISK *disk, UKE_GPT_WORKSPACE *workspace,
                        UKE_GPT_RESULT *result);
uint32_t uke_crc32(const void *data, size_t bytes);
const char *uke_status_name(UKE_STATUS status);

typedef enum { UKE_MANAGER_NONE = 0, UKE_MANAGER_REFIND, UKE_MANAGER_SYSTEMD_BOOT,
  UKE_MANAGER_DISCOVERED } UKE_MANAGER;
typedef struct {
  char profile[UKE_PROFILE_BYTES];
  UKE_MANAGER manager;
  uint8_t sha256[32];
  char path[UKE_MANAGER_PATH_BYTES]; /* Actual installed path for automatic discovery. */
} UKE_BOOT_CONFIG;
/* Strict ASCII format: schema=1, profile, manager and sha256, exactly once each. */
UKE_STATUS uke_config_parse(const void *data, size_t bytes, const char *profile,
                            UKE_BOOT_CONFIG *config);
int uke_esp_path_valid(const char *path); /* Bounded EFI/vendor/file ASCII paths. */
const char *uke_manager_path(const UKE_BOOT_CONFIG *config);
void uke_sha256(const void *data, size_t bytes, uint8_t digest[32]);
/* Structural preflight, not a replacement for firmware LoadImage authentication. */
UKE_STATUS uke_image_check(const void *data, size_t bytes, const uint8_t digest[32]);

typedef enum { UKE_BOOT_ANDROID = 0, UKE_BOOT_MANAGER } UKE_BOOT_ACTION;
typedef struct {
  int admitted_internal;
  const UKE_GPT_RESULT *gpt;
  UKE_STATUS scan_status;
} UKE_MEDIA;
typedef UKE_STATUS (*UKE_VERIFY_ESP)(void *context, size_t media,
                                     const UKE_PARTITION *partition,
                                     UKE_BOOT_CONFIG *config);
typedef struct {
  const UKE_MEDIA *media;
  size_t media_count;
  const uint8_t *bound_partition_guid; /* NULL for initial, unbound discovery. */
  void *verify_context;
  UKE_VERIFY_ESP verify;
} UKE_DISCOVERY;
typedef struct {
  UKE_BOOT_ACTION action;
  UKE_STATUS reason;
  size_t media;
  UKE_PARTITION partition;
  UKE_BOOT_CONFIG config;
} UKE_BOOT_DECISION;
/* Decisions default to Android. Eligibility is established by a real FS/image backend. */
UKE_BOOT_DECISION uke_boot_decide(const UKE_DISCOVERY *discovery);
/* Returned managers always request Android; no persistent state is changed. */
void uke_manager_return(UKE_BOOT_DECISION *decision);

typedef struct {
  void *context;
  int (*android_ready)(void *context);
  UKE_STATUS (*launch_manager)(void *context, const UKE_BOOT_DECISION *decision);
  UKE_STATUS (*resume_android)(void *context, UKE_STATUS reason);
} UKE_DISPATCH_BACKEND;
/* Gate before discovery, at most one launch and one Android continuation.
 * A successful Android backend does not return. A returned UKE_OK is therefore
 * converted to BACKEND_UNAVAILABLE, never reported as a successful Android boot. */
UKE_STATUS uke_boot_dispatch(const UKE_DISCOVERY *discovery,
                            const UKE_DISPATCH_BACKEND *backend,
                            UKE_BOOT_DECISION *decision);

typedef struct {
  const UKE_DISK *disk;
  uint64_t offset;
  uint32_t sectors, fat_sectors, data_sector, clusters, root_cluster;
  uint16_t reserved_sectors;
  uint8_t sectors_per_cluster, fat_count, active_fat, mirrored;
} UKE_FAT32;
/* Read-only FAT32 with bounded ASCII short/VFAT names; no filesystem repair. */
UKE_STATUS uke_fat32_open(const UKE_DISK *disk, const UKE_PARTITION *partition,
                           UKE_FAT32 *volume);
UKE_STATUS uke_fat32_read(const UKE_FAT32 *volume, const char *owned_path,
                           void *buffer, size_t capacity, size_t *bytes);
/* Default BOOTAA64 first, then one valid application under an EFI vendor; legacy config last.
 * Captures the selected hash; the dispatcher re-probes before loading. */
UKE_STATUS uke_esp_manager_probe(const UKE_FAT32 *volume, const char *profile,
  void *image, size_t capacity, UKE_BOOT_CONFIG *config, size_t *bytes);

typedef enum { UKE_KEY_UP = 0, UKE_KEY_DOWN, UKE_KEY_POWER, UKE_KEY_COUNT } UKE_KEY;
typedef enum { UKE_EVENT_NONE = 0, UKE_EVENT_PREVIOUS, UKE_EVENT_NEXT, UKE_EVENT_SELECT } UKE_KEY_EVENT;
typedef struct {
  uint64_t changed_ms;
  uint64_t pressed_ms;
  uint64_t next_repeat_ms;
  uint8_t sampled;
  uint8_t stable;
  uint8_t armed;
} UKE_KEY_STATE;
typedef struct {
  uint64_t last_ms;
  UKE_KEY_STATE keys[UKE_KEY_COUNT];
} UKE_BUTTONS;
/* Power selects only on a short, debounced release; initial held keys are suppressed. */
void uke_buttons_init(UKE_BUTTONS *buttons, uint64_t now_ms, uint8_t held_mask);
void uke_buttons_poll(UKE_BUTTONS *buttons, uint64_t now_ms, uint8_t held_mask,
                       UKE_KEY_EVENT events[UKE_KEY_COUNT]);

#ifdef __cplusplus
}
#endif
#endif
