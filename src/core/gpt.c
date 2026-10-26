/* SPDX-License-Identifier: MIT */
#include "uke/boot_core.h"

static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint64_t le64(const uint8_t *p) {
  return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32;
}
static int equal(const uint8_t *a, const uint8_t *b, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
  return 1;
}
static int zero_guid(const uint8_t *p) {
  static const uint8_t zero[16] = {0};
  return equal(p, zero, 16);
}
static void copy(uint8_t *a, const uint8_t *b, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) a[i] = b[i];
}
static uint32_t crc_update(uint32_t crc, const uint8_t *p, size_t n) {
  size_t i;
  unsigned bit;
  for (i = 0; i < n; ++i) {
    crc ^= p[i];
    for (bit = 0; bit < 8; ++bit)
      crc = (crc >> 1) ^ ((0u - (crc & 1u)) & 0xedb88320u);
  }
  return crc;
}
uint32_t uke_crc32(const void *data, size_t bytes) {
  return ~crc_update(0xffffffffu, (const uint8_t *)data, bytes);
}
static int read_bytes(const UKE_DISK *d, uint64_t offset, size_t n, void *out) {
  return offset <= d->bytes && (uint64_t)n <= d->bytes - offset &&
         d->read(d->context, offset, n, out);
}
typedef struct {
  uint64_t current, alternate, first, last, array;
  uint8_t guid[16];
  uint32_t entries, stride, crc;
} HEADER;
static UKE_STATUS header(const UKE_DISK *d, uint8_t *b, uint64_t lba, HEADER *h) {
  uint32_t size, stored;
  uint64_t blocks = d->bytes / d->block_size, array_bytes, array_blocks;
  if (!read_bytes(d, lba * d->block_size, d->block_size, b)) return UKE_IO_ERROR;
  if (!equal(b, (const uint8_t *)"EFI PART", 8) || le32(b + 8) != 0x00010000u ||
      le32(b + 20) != 0) return UKE_INVALID_GPT;
  size = le32(b + 12);
  if (size < 92 || size > d->block_size) return UKE_INVALID_GPT;
  stored = le32(b + 16);
  b[16] = b[17] = b[18] = b[19] = 0;
  if (uke_crc32(b, size) != stored) return UKE_INVALID_GPT;
  h->current = le64(b + 24); h->alternate = le64(b + 32);
  h->first = le64(b + 40); h->last = le64(b + 48);
  copy(h->guid, b + 56, 16);
  h->array = le64(b + 72); h->entries = le32(b + 80);
  h->stride = le32(b + 84); h->crc = le32(b + 88);
  if (h->current != lba || h->alternate != (lba == 1 ? blocks - 1 : 1) ||
      h->first < 2 || h->first > h->last || h->last >= blocks - 1 || zero_guid(h->guid))
    return UKE_INVALID_GPT;
  if (!h->entries || h->entries > UKE_MAX_PARTITIONS || h->stride != 128)
    return UKE_UNSUPPORTED_GEOMETRY;
  array_bytes = (uint64_t)h->entries * h->stride;
  array_blocks = (array_bytes + d->block_size - 1) / d->block_size;
  if (h->array >= blocks || array_blocks > blocks - h->array) return UKE_INVALID_GPT;
  if (lba == 1) {
    if (h->array < 2 || h->array + array_blocks > h->first) return UKE_INVALID_GPT;
  } else if (h->array <= h->last || h->array + array_blocks > lba) return UKE_INVALID_GPT;
  return UKE_OK;
}

UKE_STATUS uke_gpt_scan(const UKE_DISK *d, UKE_GPT_WORKSPACE *w, UKE_GPT_RESULT *r) {
  static const uint8_t esp[16] = {0x28,0x73,0x2a,0xc1,0x1f,0xf8,0xd2,0x11,
                                0xba,0x4b,0x00,0xa0,0xc9,0x3e,0xc9,0x3b};
  HEADER p, q;
  UKE_STATUS status;
  uint64_t blocks;
  uint32_t i, j, used = 0, count = 0, named = 0, crc = 0xffffffffu, protected_entries = 0;
  uint8_t a[128], b[128];
  if (r) { r->count = 0; r->named_esp_mask = 0; for (i = 0; i < 16; ++i) r->disk_guid[i] = 0; }
  if (!d || !w || !r || !d->read) return UKE_INVALID_ARGUMENT;
  if (d->block_size < 512 || d->block_size > UKE_MAX_BLOCK ||
      (d->block_size & (d->block_size - 1)) || d->bytes % d->block_size)
    return UKE_UNSUPPORTED_GEOMETRY;
  blocks = d->bytes / d->block_size;
  if (blocks < 8) return UKE_UNSUPPORTED_GEOMETRY;
  if (!read_bytes(d, 0, 512, w->block)) return UKE_IO_ERROR;
  if (w->block[510] != 0x55 || w->block[511] != 0xaa) return UKE_INVALID_MBR;
  for (i = 0; i < 4; ++i) {
    const uint8_t *m = w->block + 446 + i * 16;
    if (m[4] == 0xee && le32(m + 8) == 1 && le32(m + 12) ==
        (blocks - 1 > 0xffffffffu ? 0xffffffffu : (uint32_t)(blocks - 1))) ++protected_entries;
    else for (j = 0; j < 16; ++j) if (m[j]) return UKE_INVALID_MBR;
  }
  if (protected_entries != 1) return UKE_INVALID_MBR;
  status = header(d, w->block, 1, &p);
  if (status != UKE_OK) return status;
  status = header(d, w->block, blocks - 1, &q);
  if (status != UKE_OK) return status;
  if (!equal(p.guid, q.guid, 16) || p.first != q.first || p.last != q.last ||
      p.entries != q.entries || p.stride != q.stride || p.crc != q.crc) return UKE_GPT_MISMATCH;
  for (i = 0; i < p.entries; ++i) {
    UKE_PARTITION entry;
    if (!read_bytes(d, p.array * d->block_size + (uint64_t)i * 128, 128, a) ||
        !read_bytes(d, q.array * d->block_size + (uint64_t)i * 128, 128, b)) return UKE_IO_ERROR;
    if (!equal(a, b, 128)) return UKE_GPT_MISMATCH;
    crc = crc_update(crc, a, 128);
    if (zero_guid(a)) continue;
    copy(entry.guid, a + 16, 16);
    entry.first_lba = le64(a + 32); entry.last_lba = le64(a + 40);
    if (zero_guid(entry.guid) || entry.first_lba < p.first || entry.last_lba > p.last ||
        entry.first_lba > entry.last_lba) return UKE_INVALID_GPT;
    for (j = 0; j < used; ++j) {
      if (equal(w->used[j].guid, entry.guid, 16)) return UKE_INVALID_GPT;
      if (entry.first_lba <= w->used[j].last_lba && w->used[j].first_lba <= entry.last_lba)
        return UKE_PARTITION_OVERLAP;
    }
    w->used[used++] = entry;
    if (equal(a, esp, 16)) {
      if (count == UKE_MAX_ESPS) return UKE_TOO_MANY_ESPS;
      int label = (a[56] == 'E' || a[56] == 'e') && !a[57] &&
                  (a[58] == 'S' || a[58] == 's') && !a[59] &&
                  (a[60] == 'P' || a[60] == 'p') && !a[61];
      for (j = 62; j < 128; ++j) if (a[j]) label = 0;
      if (label) named |= 1u << count;
      r->esps[count++] = entry;
    }
  }
  if (~crc != p.crc) return UKE_INVALID_GPT;
  copy(r->disk_guid, p.guid, 16);
  r->count = count;
  r->named_esp_mask = named;
  return UKE_OK;
}

const char *uke_status_name(UKE_STATUS s) {
  static const char *const names[] = {
    "ok", "invalid-argument", "io-error", "unsupported-geometry", "invalid-mbr",
    "invalid-gpt", "gpt-mismatch", "partition-overlap", "too-many-esps", "no-esp",
    "ambiguous-esp", "bound-esp-missing", "invalid-config", "wrong-profile",
    "invalid-image", "hash-mismatch", "backend-unavailable", "external-media", "manager-returned",
    "invalid-filesystem", "file-missing", "file-too-large", "directory-ambiguous",
    "ambiguous-manager"
  };
  return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "unknown";
}
