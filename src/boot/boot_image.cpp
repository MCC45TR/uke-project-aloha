// SPDX-License-Identifier: MIT
#include "uke/boot_image.hpp"
#include "uke/boot_core.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace uke {
namespace {
constexpr size_t page = 4096;
constexpr size_t limit = 128 * 1024 * 1024;
uint32_t le32(std::span<const uint8_t> b, size_t n) {
  uint32_t v = 0;
  for (size_t i = 0; i < 4; ++i) v |= uint32_t(b[n + i]) << (8 * i);
  return v;
}
uint64_t le64(std::span<const uint8_t> b, size_t n) {
  uint64_t v = 0;
  for (size_t i = 0; i < 8; ++i) v |= uint64_t(b[n + i]) << (8 * i);
  return v;
}
uint64_t be(std::span<const uint8_t> b, size_t n, size_t bytes) {
  uint64_t v = 0;
  for (size_t i = 0; i < bytes; ++i) v = (v << 8) | b[n + i];
  return v;
}
void put(Bytes& b, size_t n, uint64_t v, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) b[n + i] = uint8_t(v >> (8 * i));
}
bool zero(std::span<const uint8_t> b) {
  return std::all_of(b.begin(), b.end(), [](uint8_t v) { return v == 0; });
}
bool fail(std::string& error, const char* message) { error = message; return false; }
uint64_t aligned(uint64_t n) { return (n + page - 1) & ~uint64_t(page - 1); }
bool branch(uint32_t insn) { return (insn & 0xfc000000u) == 0x14000000u; }
bool branch_target(uint32_t insn, uint64_t pc, uint64_t bytes, uint64_t& target) {
  int64_t delta = insn & 0x03ffffffu;
  if ((delta & (1LL << 25)) != 0) delta -= (1LL << 26);
  const auto t = static_cast<int64_t>(pc) + delta * 4;
  if (t < 64 || static_cast<uint64_t>(t) >= bytes) return false;
  target = static_cast<uint64_t>(t);
  return true;
}
bool vbmeta_length(std::span<const uint8_t> bytes, size_t& length) {
  if (bytes.size() < 256 || std::memcmp(bytes.data(), "AVB0", 4) != 0 ||
      be(bytes, 4, 4) != 1) return false;
  const auto auth = be(bytes, 12, 8), aux = be(bytes, 20, 8);
  if (auth % 64 != 0 || aux % 64 != 0 || auth > bytes.size() - 256 ||
      aux > bytes.size() - 256 - auth) return false;
  length = static_cast<size_t>(256 + auth + aux);
  return true; // Structural bounds only, never signature authentication.
}
bool embedded_metadata(std::span<const uint8_t> bytes, uint32_t& count) {
  count = 0;
  if (bytes.empty()) return true;
  if (bytes.size() > 16384 || bytes.size() % page != 0) return false;
  size_t offset = 0;
  while (!zero(bytes.subspan(offset))) {
    size_t length = 0;
    if (++count > 2 || !vbmeta_length(bytes.subspan(offset), length)) return false;
    offset += length;
  }
  return count != 0;
}
bool template_valid(std::span<const uint8_t> s) {
  if (s.size() != 256 || le32(s, 0) != 0x14000020u ||
      le32(s, 4) != 0xd503201fu || std::memcmp(s.data() + 8, "UKEAND01", 8) != 0 ||
      le32(s, 16) != 1 || le32(s, 20) != 256 || !zero(s.subspan(24, 104)) ||
      !zero(s.subspan(156))) return false;
  // Exact, position-independent seven-instruction Android-only stub. There is
  // no indirect firmware entry in this format. The build script checks it too.
  constexpr std::array<uint32_t, 7> code {
    0x10fffc10u, 0x58fffcb1u, 0xcb110210u, 0x58fffcb1u,
    0x8b110210u, 0xd503201fu, 0xd61f0200u
  };
  for (size_t i = 0; i < code.size(); ++i)
    if (le32(s, 128 + i * 4) != code[i]) return false;
  return true;
}
}

std::string sha256_hex(std::span<const uint8_t> data) {
  std::array<uint8_t, 32> hash {};
  uke_sha256(data.data(), data.size(), hash.data());
  constexpr char digits[] = "0123456789abcdef";
  std::string hex(64, '0');
  for (size_t i = 0; i < hash.size(); ++i) {
    hex[2 * i] = digits[hash[i] >> 4]; hex[2 * i + 1] = digits[hash[i] & 15];
  }
  return hex;
}

bool inspect_boot(std::span<const uint8_t> b, BootLayout& out, std::string& error) {
  out = {}; error.clear();
  if (b.size() < page * 2 || b.size() > limit || b.size() % page != 0 ||
      std::memcmp(b.data(), "ANDROID!", 8) != 0) return fail(error, "invalid boot container or capacity");
  if (le32(b, 40) != 4 || le32(b, 20) != 1584 || !zero(b.subspan(24, 16)))
    return fail(error, "only canonical boot header v4 is supported");
  if (le32(b, 12) != 0 || le32(b, 1580) != 0)
    return fail(error, "ramdisk or boot signature requires a separate repack/signing contract");
  if (!zero(b.subspan(1584, page - 1584))) return fail(error, "nonzero boot-header padding");
  const auto kernel = le32(b, 8);
  if (kernel < 64 || kernel > b.size() - page) return fail(error, "kernel exceeds container");
  const auto k = b.subspan(page, kernel);
  if (le32(k, 56) != 0x644d5241u || le64(k, 16) == 0 ||
      le64(k, 16) > limit || (le64(k, 24) & 1) != 0 ||
      !zero(k.subspan(32, 24))) return fail(error, "unsupported AArch64 Image header");
  const auto end = page + aligned(kernel);
  if (end > b.size() || !zero(b.subspan(page + kernel, static_cast<size_t>(end - page - kernel))))
    return fail(error, "nonzero kernel padding");
  bool footer = false;
  uint32_t embedded = 0;
  const size_t f = b.size() - 64;
  if (std::memcmp(b.data() + f, "AVBf", 4) == 0) {
    footer = true;
    const auto original = be(b, f + 12, 8);
    const auto offset = be(b, f + 20, 8);
    const auto length = be(b, f + 28, 8);
    size_t meta_length = 0;
    if (be(b, f + 4, 4) != 1 || be(b, f + 8, 4) != 0 || original < end ||
        original > f || !embedded_metadata(b.subspan(static_cast<size_t>(end), static_cast<size_t>(original - end)), embedded) ||
        offset < original || offset > f || length < 256 || length > f - offset ||
        !zero(b.subspan(f + 36, 28)) ||
        !vbmeta_length(b.subspan(static_cast<size_t>(offset), static_cast<size_t>(length)), meta_length) ||
        meta_length != length ||
        !zero(b.subspan(static_cast<size_t>(original), static_cast<size_t>(offset - original))) ||
        !zero(b.subspan(static_cast<size_t>(offset + length), f - static_cast<size_t>(offset + length))))
      return fail(error, "invalid AVB footer, bounds or partition padding");
  } else if (!zero(b.subspan(static_cast<size_t>(end)))) {
    return fail(error, "unknown trailing boot data");
  }
  out = {kernel, le64(k, 16), end, footer, embedded}; return true;
}

static bool stage_payload(std::span<const uint8_t>, std::span<const uint8_t>,
                          const std::string&, const std::string&, Bytes&, std::string&);

bool stage_android_continuation(std::span<const uint8_t> original,
                                std::span<const uint8_t> selector,
                                const std::string& expected, Bytes& result,
                                std::string& error) {
  if (!template_valid(selector)) {
    result.clear(); return fail(error, "invalid Android-continuation selector template");
  }
  return stage_payload(original, selector, expected, sha256_hex(selector), result, error);
}

bool stage_sec_probe(std::span<const uint8_t> original, std::span<const uint8_t> selector,
                     const std::string& expected, const std::string& probe_hash,
                     Bytes& result, std::string& error) {
  if (selector.size() < 128 || std::memcmp(selector.data() + 8, "UKESPR01", 8)) {
    result.clear(); return fail(error, "SEC-probe payload required");
  }
  return stage_payload(original, selector, expected, probe_hash, result, error);
}

static bool stage_payload(std::span<const uint8_t> original, std::span<const uint8_t> selector,
                          const std::string& expected, const std::string& probe_hash,
                          Bytes& result, std::string& error) {
  result.clear();
  if (expected.size() != 64 || sha256_hex(original) != expected)
    return fail(error, "stock boot SHA-256 does not match the selected input");
  BootLayout layout;
  if (!inspect_boot(original, layout, error)) return false;
  const bool continuation = template_valid(selector);
  const auto replay_offset = continuation ? 148u : selector.size() >= 128 ? le32(selector, 60) : 0u;
  if (probe_hash.size() != 64 || sha256_hex(selector) != probe_hash)
    return fail(error, "payload SHA-256 does not match the selected build");
  if (!continuation && (selector.size() < page || selector.size() > 512 * 1024 ||
      selector.size() % page || le32(selector, 0) != 0x14000020u ||
      le32(selector, 4) != 0xd503201fu || std::memcmp(selector.data() + 8, "UKESPR01", 8) ||
      le32(selector, 16) != 1 || le32(selector, 20) != selector.size() ||
      !zero(selector.subspan(24, 36)) || !zero(selector.subspan(64, 64)) ||
      replay_offset < 128 || replay_offset > selector.size() - 8 || replay_offset % 4 ||
      le32(selector, replay_offset) != 0xd503201fu || le32(selector, replay_offset + 4) != 0xd61f0200u))
    return fail(error, "invalid SEC-probe payload");
  const auto k = original.subspan(page, layout.kernel_bytes);
  const auto code0 = le32(k, 0);
  uint64_t resume = 0;
  uint32_t replay = 0xd503201fu;
  if (branch(code0)) {
    if (!branch_target(code0, 0, layout.kernel_bytes, resume)) return fail(error, "invalid original entry branch");
    if (resume + 16 <= k.size() &&
        (std::memcmp(k.data() + resume + 8, "UKEAND01", 8) == 0 ||
         std::memcmp(k.data() + resume + 8, "UKESPR01", 8) == 0))
      return fail(error, "kernel already contains the development continuation");
  } else if (code0 == 0xfa405a4du && branch(le32(k, 4))) {
    uint64_t destination = 0;
    if (!branch_target(le32(k, 4), 4, layout.kernel_bytes, destination)) return fail(error, "invalid EFI-stub entry branch");
    resume = 4; replay = code0; // Replay the original non-PC-relative MZ instruction.
  } else return fail(error, "unrecognized kernel entry instructions");
  const auto offset = aligned(std::max<uint64_t>(layout.kernel_bytes, layout.image_bytes));
  const auto new_size = aligned(offset + selector.size());
  if (offset >= (1u << 27) || new_size > original.size() - page ||
      new_size > std::numeric_limits<uint32_t>::max()) return fail(error, "selector branch or container capacity exceeded");
  result.assign(original.size(), 0); // Stale AVB metadata must not survive a changed kernel.
  std::copy_n(original.begin(), page + layout.kernel_bytes, result.begin());
  put(result, 8, new_size, 4);
  put(result, page, 0x14000000u | (offset / 4), 4);
  put(result, page + 16, new_size, 8);
  std::copy(selector.begin(), selector.end(), result.begin() + static_cast<ptrdiff_t>(page + offset));
  const auto s = static_cast<size_t>(page + offset);
  put(result, s + 24, offset, 8); put(result, s + 32, resume, 8);
  put(result, s + 40, layout.kernel_bytes, 8); put(result, s + 48, layout.image_bytes, 8);
  put(result, s + 56, code0, 4); put(result, s + replay_offset, replay, 4);
  std::array<uint8_t, 32> digest {};
  uke_sha256(original.data(), original.size(), digest.data());
  std::copy(digest.begin(), digest.end(), result.begin() + static_cast<ptrdiff_t>(s + 64));
  uke_sha256(selector.data(), selector.size(), digest.data());
  std::copy(digest.begin(), digest.end(), result.begin() + static_cast<ptrdiff_t>(s + 96));
  error.clear(); return true;
}

bool restore_sec_probe(std::span<const uint8_t> backup, std::span<const uint8_t> staged,
                       std::span<const uint8_t> probe, const std::string& boot_hash,
                       const std::string& probe_hash, Bytes& result, std::string& error) {
  result.clear(); Bytes regenerated;
  if (!stage_sec_probe(backup, probe, boot_hash, probe_hash, regenerated, error)) return false;
  if (staged.size() != regenerated.size() || !std::equal(staged.begin(), staged.end(), regenerated.begin()))
    return fail(error, "staged probe does not match the exact backup/build relationship");
  result.assign(backup.begin(), backup.end()); error.clear(); return true;
}

bool restore_android_continuation(std::span<const uint8_t> backup,
                                  std::span<const uint8_t> staged,
                                  std::span<const uint8_t> selector,
                                  const std::string& expected, Bytes& result,
                                  std::string& error) {
  result.clear(); Bytes regenerated;
  if (!stage_android_continuation(backup, selector, expected, regenerated, error)) return false;
  if (staged.size() != regenerated.size() || !std::equal(staged.begin(), staged.end(), regenerated.begin()))
    return fail(error, "staged image does not match the exact backup/template relationship");
  result.assign(backup.begin(), backup.end()); error.clear(); return true;
}
}
