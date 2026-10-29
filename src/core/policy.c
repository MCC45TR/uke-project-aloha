/* SPDX-License-Identifier: MIT */
#include "uke/boot_core.h"

static int same(const uint8_t *a, const uint8_t *b, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) if (a[i] != b[i]) return 0;
  return 1;
}
static int text(const uint8_t *p, size_t n, const char *s) {
  size_t i = 0;
  while (s[i]) { if (i >= n || p[i] != (uint8_t)s[i]) return 0; ++i; }
  return i == n;
}
static int hex(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
UKE_STATUS uke_config_parse(const void *data, size_t n, const char *profile, UKE_BOOT_CONFIG *out) {
  const uint8_t *p = (const uint8_t *)data;
  UKE_BOOT_CONFIG c = {0};
  size_t pos = 0, start, split, end, i;
  unsigned seen = 0, field;
  if (!p || !profile || !out || !n || n > UKE_MAX_CONFIG) return UKE_INVALID_CONFIG;
  while (pos < n) {
    start = pos; split = n;
    while (pos < n && p[pos] != '\n') {
      if (p[pos] < 0x21 || p[pos] > 0x7e) return UKE_INVALID_CONFIG;
      if (p[pos] == '=') { if (split != n) return UKE_INVALID_CONFIG; split = pos; }
      ++pos;
    }
    end = pos; if (pos < n) ++pos;
    if (split == n || split == start || split + 1 == end) return UKE_INVALID_CONFIG;
    if (text(p + start, split - start, "schema")) {
      field = 1; if (!text(p + split + 1, end - split - 1, "1")) return UKE_INVALID_CONFIG;
    } else if (text(p + start, split - start, "profile")) {
      field = 2;
      if (end - split - 1 >= UKE_PROFILE_BYTES) return UKE_INVALID_CONFIG;
      for (i = split + 1; i < end; ++i) {
        if (!((p[i] >= 'a' && p[i] <= 'z') || (p[i] >= 'A' && p[i] <= 'Z') ||
              (p[i] >= '0' && p[i] <= '9') || p[i] == '.' || p[i] == '-' || p[i] == '_'))
          return UKE_INVALID_CONFIG;
        c.profile[i - split - 1] = (char)p[i];
      }
    } else if (text(p + start, split - start, "manager")) {
      field = 4;
      if (text(p + split + 1, end - split - 1, "refind")) c.manager = UKE_MANAGER_REFIND;
      else if (text(p + split + 1, end - split - 1, "systemd-boot")) c.manager = UKE_MANAGER_SYSTEMD_BOOT;
      else return UKE_INVALID_CONFIG;
    } else if (text(p + start, split - start, "sha256")) {
      field = 8; if (end - split - 1 != 64) return UKE_INVALID_CONFIG;
      for (i = 0; i < 32; ++i) {
        int a = hex(p[split + 1 + i * 2]), b = hex(p[split + 2 + i * 2]);
        if (a < 0 || b < 0) return UKE_INVALID_CONFIG;
        c.sha256[i] = (uint8_t)(a * 16 + b);
      }
    } else return UKE_INVALID_CONFIG;
    if (seen & field) return UKE_INVALID_CONFIG;
    seen |= field;
  }
  if (seen != 15) return UKE_INVALID_CONFIG;
  i = 0; while (i < UKE_PROFILE_BYTES && c.profile[i]) ++i;
  if (!text((const uint8_t *)c.profile, i, profile)) return UKE_WRONG_PROFILE;
  *out = c;
  return UKE_OK;
}
int uke_esp_path_valid(const char *p) {
  size_t i = 0, start = 0, components = 0;
  if (!p) return 0;
  while (i < UKE_MANAGER_PATH_BYTES && p[i]) {
    char c = p[i];
    if (c == '/') {
      size_t n = i - start;
      if (!n || n > 63 || p[i-1] == '.' || ++components > 2) return 0;
      start = i + 1;
    } else if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                 (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.' || c == '~')) return 0;
    ++i;
  }
  return i < UKE_MANAGER_PATH_BYTES && components == 2 && i > start &&
    i - start <= 63 && p[i-1] != '.' && (p[0] == 'E' || p[0] == 'e') &&
    (p[1] == 'F' || p[1] == 'f') && (p[2] == 'I' || p[2] == 'i') && p[3] == '/';
}
const char *uke_manager_path(const UKE_BOOT_CONFIG *c) {
  if (!c) return (const char *)0;
  if (c->manager == UKE_MANAGER_REFIND) return "EFI/UKE/REFIND.EFI";
  if (c->manager == UKE_MANAGER_SYSTEMD_BOOT) return "EFI/UKE/SDBOOT.EFI";
  if (c->manager == UKE_MANAGER_DISCOVERED && uke_esp_path_valid(c->path)) return c->path;
  return (const char *)0;
}

UKE_BOOT_DECISION uke_boot_decide(const UKE_DISCOVERY *d) {
  UKE_BOOT_DECISION answer = {0};
  answer.reason = UKE_NO_ESP;
  size_t i, j, matches = 0;
  int bound_found = 0;
  if (!d || !d->media || !d->verify || !d->media_count || d->media_count > 16) {
    answer.reason = UKE_INVALID_ARGUMENT; return answer;
  }
  for (i = 0; i < d->media_count; ++i) {
    const UKE_MEDIA *m = &d->media[i];
    if (!m->admitted_internal) continue;
    if (m->scan_status != UKE_OK || !m->gpt || m->gpt->count > UKE_MAX_ESPS) {
      answer.action = UKE_BOOT_ANDROID;
      answer.reason = m->scan_status != UKE_OK ? m->scan_status : UKE_INVALID_ARGUMENT;
      return answer;
    }
    for (j = 0; j < m->gpt->count; ++j) {
      UKE_BOOT_CONFIG c = {0};
      UKE_STATUS s;
      const UKE_PARTITION *p = &m->gpt->esps[j];
      if (!(m->gpt->named_esp_mask & (1u << j))) continue;
      if (d->bound_partition_guid && !same(d->bound_partition_guid, p->guid, 16)) continue;
      if (d->bound_partition_guid && bound_found++) {
        answer.action = UKE_BOOT_ANDROID; answer.reason = UKE_AMBIGUOUS_ESP; return answer;
      }
      s = d->verify(d->verify_context, i, p, &c);
      if (s != UKE_OK) { if (!matches) answer.reason = s; continue; }
      if (!uke_manager_path(&c)) {
        if (!matches) answer.reason = UKE_INVALID_CONFIG;
        continue;
      }
      if (++matches > 1) {
        answer.action = UKE_BOOT_ANDROID; answer.reason = UKE_AMBIGUOUS_ESP; return answer;
      }
      answer.action = UKE_BOOT_MANAGER; answer.reason = UKE_OK;
      answer.media = i; answer.partition = *p; answer.config = c;
    }
  }
  if (d->bound_partition_guid && !bound_found) answer.reason = UKE_BOUND_ESP_MISSING;
  return answer;
}
void uke_manager_return(UKE_BOOT_DECISION *d) {
  if (d) { d->action = UKE_BOOT_ANDROID; d->reason = UKE_MANAGER_RETURNED; }
}

static uint16_t le16(const uint8_t *p) { return (uint16_t)((uint32_t)p[0] | (uint32_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) {
  return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
UKE_STATUS uke_image_check(const void *data, size_t n, const uint8_t expected[32]) {
  const uint8_t *p = (const uint8_t *)data;
  uint32_t pe, headers, image_size, entry, section_alignment, file_alignment;
  size_t sections, opt, table, i;
  int executable_entry = 0;
  uint8_t digest[32];
  if (!p || !expected || n < 64 || n > UKE_MAX_IMAGE || p[0] != 'M' || p[1] != 'Z')
    return UKE_INVALID_IMAGE;
  pe = le32(p + 60);
  if (pe < 64 || pe > n || n - pe < 24 || !same(p + pe, (const uint8_t *)"PE\0\0", 4) ||
      le16(p + pe + 4) != 0xaa64 || !(le16(p + pe + 22) & 2)) return UKE_INVALID_IMAGE;
  sections = le16(p + pe + 6); opt = le16(p + pe + 20);
  if (!sections || sections > 96 || opt < 112 || opt > n - pe - 24) return UKE_INVALID_IMAGE;
  table = (size_t)pe + 24 + opt;
  if (sections > (n - table) / 40 || le16(p + pe + 24) != 0x20b ||
      le16(p + pe + 24 + 68) != 10) return UKE_INVALID_IMAGE;
  entry = le32(p + pe + 24 + 16);
  section_alignment = le32(p + pe + 24 + 32); file_alignment = le32(p + pe + 24 + 36);
  image_size = le32(p + pe + 24 + 56); headers = le32(p + pe + 24 + 60);
  if (!section_alignment || (section_alignment & (section_alignment - 1)) ||
      !file_alignment || (file_alignment & (file_alignment - 1)) || file_alignment > section_alignment ||
      !image_size || image_size > UKE_MAX_IMAGE || entry >= image_size ||
      headers < table + sections * 40 || headers > n) return UKE_INVALID_IMAGE;
  for (i = 0; i < sections; ++i) {
    const uint8_t *s = p + table + i * 40;
    uint32_t virtual_bytes = le32(s + 8), rva = le32(s + 12);
    uint32_t raw_bytes = le32(s + 16), raw = le32(s + 20);
    uint32_t mapped = virtual_bytes > raw_bytes ? virtual_bytes : raw_bytes;
    if (rva > image_size || mapped > image_size - rva ||
        (raw_bytes && (raw < headers || raw > n || raw_bytes > n - raw))) return UKE_INVALID_IMAGE;
    if ((le32(s + 36) & 0x20000000u) && entry >= rva && entry - rva < raw_bytes)
      executable_entry = 1;
  }
  if (!executable_entry) return UKE_INVALID_IMAGE;
  uke_sha256(data, n, digest);
  return same(digest, expected, 32) ? UKE_OK : UKE_HASH_MISMATCH;
}
