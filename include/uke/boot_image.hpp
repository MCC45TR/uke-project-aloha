// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace uke {
using Bytes = std::vector<uint8_t>;
struct BootLayout {
  uint32_t kernel_bytes = 0;
  uint64_t image_bytes = 0;
  uint64_t body_end = 0;
  bool avb_footer = false;
  uint32_t embedded_vbmeta = 0;
};
// Deliberately restricted to v4, uncompressed AArch64, kernel-only boot images.
// No partition devices, compression, init_boot/vendor_boot changes or signing.
bool inspect_boot(std::span<const uint8_t>, BootLayout&, std::string& error);
bool stage_android_continuation(std::span<const uint8_t> original,
                                std::span<const uint8_t> selector,
                                const std::string& expected_sha256,
                                Bytes& result, std::string& error);
bool restore_android_continuation(std::span<const uint8_t> original_backup,
                                  std::span<const uint8_t> staged,
                                  std::span<const uint8_t> selector,
                                  const std::string& expected_sha256,
                                  Bytes& result, std::string& error);
bool stage_sec_probe(std::span<const uint8_t> original, std::span<const uint8_t> probe,
                     const std::string& boot_sha256, const std::string& probe_sha256,
                     Bytes& result, std::string& error);
bool restore_sec_probe(std::span<const uint8_t> backup, std::span<const uint8_t> staged,
                       std::span<const uint8_t> probe, const std::string& boot_sha256,
                       const std::string& probe_sha256, Bytes& result, std::string& error);
std::string sha256_hex(std::span<const uint8_t>);
}
