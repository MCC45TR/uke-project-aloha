// SPDX-License-Identifier: MIT
#include "uke/boot_image.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>

namespace {
size_t checks = 0;
void require(bool good, const char* message) {
  ++checks;
  if (!good) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
void put(uke::Bytes& b, size_t offset, uint64_t value, size_t bytes = 4, bool big = false) {
  for (size_t i = 0; i < bytes; ++i)
    b[offset + i] = uint8_t(value >> (8 * (big ? bytes - i - 1 : i)));
}
void magic(uke::Bytes& b, size_t offset, const char* text, size_t bytes) {
  std::memcpy(b.data() + offset, text, bytes);
}
uke::Bytes selector() {
  uke::Bytes b(256);
  put(b, 0, 0x14000020); put(b, 4, 0xd503201f);
  magic(b, 8, "UKEAND01", 8); put(b, 16, 1); put(b, 20, 256);
  constexpr std::array<uint32_t, 7> code {0x10fffc10, 0x58fffcb1, 0xcb110210,
    0x58fffcb1, 0x8b110210, 0xd503201f, 0xd61f0200};
  for (size_t i = 0; i < code.size(); ++i) put(b, 128 + i * 4, code[i]);
  return b;
}
uke::Bytes boot(bool mz, bool avb) {
  uke::Bytes b(32768);
  magic(b, 0, "ANDROID!", 8); put(b, 8, 4096); put(b, 20, 1584); put(b, 40, 4);
  put(b, 4096, mz ? 0xfa405a4d : 0x14000010); put(b, 4100, 0x1400000f);
  put(b, 4112, 8192, 8); put(b, 4152, 0x644d5241);
  for (size_t i = 4160; i < 8192; ++i) b[i] = uint8_t(i % 251);
  if (avb) {
    // Two embedded metadata objects despite signature_size=0, matching the
    // structural OS3 finding. Fixtures contain no signature or private data.
    for (size_t offset : {size_t(8192), size_t(8448), size_t(12288)}) {
      magic(b, offset, "AVB0", 4); put(b, offset + 4, 1, 4, true);
    }
    const size_t f = b.size() - 64;
    magic(b, f, "AVBf", 4); put(b, f + 4, 1, 4, true);
    put(b, f + 12, 12288, 8, true); put(b, f + 20, 12288, 8, true);
    put(b, f + 28, 256, 8, true);
  }
  return b;
}
}

int main(int argc, char **argv) {
  std::string error; uke::BootLayout layout; uke::Bytes result, restored;
  if (argc == 4 && std::strcmp(argv[1], "--write-probe-fixture") == 0) {
    std::ifstream input(argv[2], std::ios::binary);
    uke::Bytes image((std::istreambuf_iterator<char>(input)), {});
    require(image.size() == 4096, "bounded synthetic Image");
    auto container = boot(false, false); container.resize(512 * 1024);
    std::copy(image.begin(), image.end(), container.begin() + 4096);
    require(uke::inspect_boot(container, layout, error), "synthetic boot container");
    std::ofstream output(argv[3], std::ios::binary);
    output.write(reinterpret_cast<const char *>(container.data()), static_cast<std::streamsize>(container.size()));
    require(output.good(), "write synthetic boot"); return 0;
  }
  const auto stub = selector();
  for (bool mz : {false, true}) for (bool avb : {false, true}) {
    const auto source = boot(mz, avb); const auto hash = uke::sha256_hex(source);
    require(uke::inspect_boot(source, layout, error), "valid container");
    require(layout.avb_footer == avb && layout.embedded_vbmeta == (avb ? 2u : 0u), "metadata classified");
    require(uke::stage_android_continuation(source, stub, hash, result, error), "stage fixture");
    require(result.size() == source.size(), "partition capacity retained");
    require(std::equal(source.begin() + 4160, source.begin() + 8192, result.begin() + 4160), "kernel body preserved");
    require(uke::inspect_boot(result, layout, error) && !layout.avb_footer && layout.embedded_vbmeta == 0, "stale metadata removed");
    require(uke::restore_android_continuation(source, result, stub, hash, restored, error) && restored == source, "byte-exact restore incl AVB");
    auto altered = result; altered[5000] ^= 1;
    require(!uke::restore_android_continuation(source, altered, stub, hash, restored, error) && restored.empty(), "damaged kernel rejected");
    altered = result; altered.back() = 1;
    require(!uke::restore_android_continuation(source, altered, stub, hash, restored, error), "damaged tail rejected");
    const auto staged_hash = uke::sha256_hex(result);
    require(!uke::stage_android_continuation(result, stub, staged_hash, restored, error), "double patch rejected");
  }
  const auto source = boot(true, true); const auto hash = uke::sha256_hex(source);
  require(!uke::stage_android_continuation(source, stub, std::string(64, '0'), result, error) && result.empty(), "wrong profile hash rejected");
  const std::array<size_t, 16> invalid_fields {0, 12, 20, 24, 40, 1580, 2000,
      4152, 4120, 4128, 8192, 8192 + 12, 12288, 32704 + 4,
      32704 + 20, 32704 + 36};
  for (auto offset : invalid_fields) {
    auto altered = source;
    altered[offset] ^= offset == 8192 + 12 ? uint8_t(0x80) : uint8_t(1);
    require(!uke::inspect_boot(altered, layout, error), "malformed field rejected");
  }
  for (size_t size : {size_t(0), size_t(8), size_t(4096), size_t(8191), size_t(32767)})
    require(!uke::inspect_boot(std::span(source).first(size), layout, error), "truncated container rejected");
  for (size_t offset : {size_t(0), size_t(8), size_t(16), size_t(24), size_t(60), size_t(128), size_t(152), size_t(255)}) {
    auto altered = stub; altered[offset] ^= 1;
    require(!uke::stage_android_continuation(source, altered, hash, result, error) && result.empty(), "altered selector rejected");
  }
  auto altered = source; put(altered, 4096, 0xd503201f);
  require(!uke::stage_android_continuation(altered, stub, uke::sha256_hex(altered), result, error), "unknown entry rejected");
  altered = boot(false, false); put(altered, 4096, 0x17ffffff);
  require(!uke::stage_android_continuation(altered, stub, uke::sha256_hex(altered), result, error), "negative entry target rejected");
  altered = boot(false, false); put(altered, 4112, 32768, 8);
  require(!uke::stage_android_continuation(altered, stub, uke::sha256_hex(altered), result, error), "insufficient capacity rejected");
  uke::Bytes probe(4096);
  put(probe, 0, 0x14000020); put(probe, 4, 0xd503201f);
  magic(probe, 8, "UKESPR01", 8); put(probe, 16, 1); put(probe, 20, probe.size());
  put(probe, 60, 256); put(probe, 256, 0xd503201f); put(probe, 260, 0xd61f0200);
  const auto probe_hash = uke::sha256_hex(probe);
  for (bool mz : {false, true}) {
    const auto input = boot(mz, true); const auto input_hash = uke::sha256_hex(input);
    require(uke::stage_sec_probe(input, probe, input_hash, probe_hash, result, error), "stage SEC probe");
    require(uke::restore_sec_probe(input, result, probe, input_hash, probe_hash, restored, error) && restored == input, "exact SEC probe restore");
    require(!uke::stage_sec_probe(result, probe, uke::sha256_hex(result), probe_hash, restored, error), "double SEC probe rejected");
    auto damaged = result; damaged[5000] ^= 1;
    require(!uke::restore_sec_probe(input, damaged, probe, input_hash, probe_hash, restored, error), "damaged SEC stage rejected");
  }
  for (size_t offset : {size_t(0), size_t(8), size_t(16), size_t(20), size_t(24), size_t(60), size_t(64), size_t(256), size_t(260)}) {
    auto bad = probe; bad[offset] ^= 1;
    require(!uke::stage_sec_probe(source, bad, hash, uke::sha256_hex(bad), result, error) && result.empty(), "invalid SEC metadata rejected even with matching hash");
  }
  require(!uke::stage_sec_probe(source, probe, hash, std::string(64, '0'), result, error), "wrong SEC build hash rejected");
  require(!uke::stage_sec_probe(source, stub, hash, uke::sha256_hex(stub), result, error), "continuation cannot masquerade as SEC probe");
  probe.resize(4095);
  require(!uke::stage_sec_probe(source, probe, hash, uke::sha256_hex(probe), result, error), "truncated SEC probe rejected");
  std::printf("boot-image checks=%zu passed\n", checks);
}
