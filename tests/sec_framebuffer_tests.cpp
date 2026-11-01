// SPDX-License-Identifier: MIT
#include "uke/sec_probe.h"
#include <libfdt.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

namespace {
unsigned checks;
void require(bool ok, const char *why) {
  ++checks;
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
void reg(void *dt, const char *path, uint64_t base, uint64_t bytes) {
  const fdt64_t data[] = {cpu_to_fdt64(base), cpu_to_fdt64(bytes)};
  require(fdt_setprop(dt, fdt_path_offset(dt, path), "reg", data, sizeof(data)) == 0, "set reg");
}
std::array<unsigned char, 8192> fixture() {
  std::array<unsigned char, 8192> data {};
  void *dt = data.data();
  require(fdt_create_empty_tree(dt, static_cast<int>(data.size())) == 0, "create tree");
  require(fdt_setprop_u32(dt, 0, "#address-cells", 2) == 0 &&
          fdt_setprop_u32(dt, 0, "#size-cells", 2) == 0, "root cells");
  const char compatible[] = "qcom,cliffs-mtp\0qcom,cliffs";
  require(fdt_setprop(dt, 0, "compatible", compatible, sizeof(compatible)) == 0, "compatible");
  require(fdt_setprop_string(dt, 0, "model", "Qualcomm Technologies, Inc. Uke based on SM8635") == 0, "model");
  int node = fdt_add_subnode(dt, 0, "memory");
  require(node >= 0 && fdt_setprop_string(dt, node, "device_type", "memory") == 0, "memory node");
  reg(dt, "/memory", 0x40000000, 0x20000000);
  node = fdt_add_subnode(dt, 0, "reserved-memory");
  require(node >= 0 && fdt_setprop_u32(dt, node, "#address-cells", 2) == 0 &&
          fdt_setprop_u32(dt, node, "#size-cells", 2) == 0 &&
          fdt_setprop(dt, node, "ranges", nullptr, 0) == 0, "reserved cells");
  require(fdt_add_subnode(dt, node, "splash_region") >= 0, "splash node");
  reg(dt, "/reserved-memory/splash_region", 0x50000000, 32 * 1024 * 1024);
  return data;
}
}
int main(int argc, char **argv) {
  uke_sec_framebuffer out {};
  if (argc == 2) {
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)), {});
    require(!bytes.empty() && bytes.size() <= 2 * 1024 * 1024, "private fixture bounded");
    require(uke_sec_admit_framebuffer(bytes.data(), bytes.size(), 0x82600000, 68 * 1024 * 1024, &out), "retained Uke FDT admitted");
    std::puts("retained-fdt=admitted; contents not exported"); return 0;
  }
  auto valid = fixture(); const auto original = valid;
  require(uke_sec_admit_framebuffer(valid.data(), valid.size(), 0x40080000, 1024 * 1024, &out), "valid dynamic framebuffer");
  require(out.address == 0x50000000 && out.bytes == 3200ULL * 2136 * 4 && valid == original, "address derived; input unchanged");
  for (unsigned kind = 0; kind < 11; ++kind) {
    auto bad = valid; void *dt = bad.data();
    if (kind == 0) bad[0] ^= 1;
    if (kind == 1) require(fdt_setprop_string(dt, 0, "model", "foreign") == 0, "foreign model");
    if (kind == 2) reg(dt, "/reserved-memory/splash_region", 0x50000001, 32 * 1024 * 1024);
    if (kind == 3) reg(dt, "/reserved-memory/splash_region", 0x50000000, 4096);
    if (kind == 4) reg(dt, "/reserved-memory/splash_region", UINT64_MAX - 4095, 32 * 1024 * 1024);
    if (kind == 5) reg(dt, "/reserved-memory/splash_region", 0x70000000, 32 * 1024 * 1024);
    if (kind == 6) require(fdt_setprop(dt, fdt_path_offset(dt, "/reserved-memory/splash_region"), "reusable", nullptr, 0) == 0, "reusable");
    if (kind == 7) require(fdt_setprop_u32(dt, 0, "#address-cells", 1) == 0, "wrong cells");
    if (kind == 8) require(fdt_setprop_string(dt, 0, "compatible", "qcom,foreign") == 0, "wrong compatible");
    if (kind == 9) {
      require(fdt_add_subnode(dt, fdt_path_offset(dt, "/reserved-memory"), "other") >= 0, "other reserved");
      reg(dt, "/reserved-memory/other", 0x50001000, 4096);
    }
    if (kind == 10) require(fdt_add_mem_rsv(dt, 0x50001000, 4096) == 0, "reservation map");
    out = {1, 1};
    require(!uke_sec_admit_framebuffer(dt, bad.size(), 0x40080000, 1024 * 1024, &out) &&
            !out.address && !out.bytes, "malformed or unsafe framebuffer rejected");
  }
  require(!uke_sec_admit_framebuffer(valid.data(), valid.size(), 0x50000000, 4096, &out), "kernel overlap rejected");
  require(!uke_sec_admit_framebuffer(valid.data(), 39, 0x40080000, 4096, &out), "truncation rejected");
  require(!uke_sec_admit_framebuffer(valid.data(), 2 * 1024 * 1024 + 1, 0x40080000, 4096, &out), "oversize rejected before read");
  require(!uke_sec_admit_framebuffer(valid.data(), valid.size(), UINT64_MAX - 3, 16, &out), "kernel overflow rejected");
  if (argc == 3 && (std::strcmp(argv[1], "--write-fixture") == 0 ||
                    std::strcmp(argv[1], "--write-foreign-fixture") == 0)) {
    if (std::strcmp(argv[1], "--write-foreign-fixture") == 0)
      require(fdt_setprop_string(valid.data(), 0, "model", "foreign") == 0, "foreign QEMU fixture");
    std::ofstream file(argv[2], std::ios::binary);
    file.write(reinterpret_cast<const char *>(valid.data()), static_cast<std::streamsize>(valid.size()));
    require(file.good(), "write synthetic QEMU fixture");
  }
  std::printf("sec-framebuffer checks=%u passed\n", checks);
}
