// SPDX-License-Identifier: MIT
#include "uke/platform_boot.h"
#include "uke/raw_entry.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
static unsigned checks = 0;
static void require(bool value, const char* text) {
  ++checks; if (!value) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}
struct Backend {
  std::array<unsigned, 40> order {}; size_t count = 0;
  bool admitted = true; unsigned fail_driver = 99;
  UKE_STATUS prepare_status = UKE_OK, exit_status = UKE_OK;
  struct Driver {Backend* parent; unsigned id;};
  static UKE_STATUS quiesce(void* opaque) {
    auto& d = *static_cast<Driver*>(opaque); d.parent->order[d.parent->count++] = d.id;
    return d.parent->fail_driver == d.id ? UKE_IO_ERROR : UKE_OK;
  }
  static int ready(void* opaque) { return static_cast<Backend*>(opaque)->admitted; }
  static UKE_STATUS prepare(void* opaque) {
    auto& b = *static_cast<Backend*>(opaque); b.order[b.count++] = 100; return b.prepare_status;
  }
  static UKE_STATUS exit(void* opaque) {
    auto& b = *static_cast<Backend*>(opaque); b.order[b.count++] = 101; return b.exit_status;
  }
  static UKE_STATUS jump(void* opaque) {
    auto& b = *static_cast<Backend*>(opaque); b.order[b.count++] = 102; return UKE_OK;
  }
};
int main() {
  const UKE_RANGE raw_kernel {0x90000000, 0x4000000}, raw_fdt {0x95000000, 0x100000};
  UKE_RAW_ENTRY entry {};
  entry.x[0] = raw_fdt.base; entry.stack = 0x97000000; entry.current_el = 4;
  entry.daif = 0x3c0; entry.cntfrq = 19200000; entry.resume = raw_kernel.base + 4;
  require(uke_sec_entry_admit(&entry, raw_kernel, raw_fdt) == UKE_OK, "narrow EL1 raw entry admitted");
  entry.current_el = 8; entry.replay_mz = 1;
  require(uke_sec_entry_admit(&entry, raw_kernel, raw_fdt) == UKE_OK, "narrow EL2 MZ raw entry admitted");
  const auto valid_entry = entry;
  for (unsigned trial = 0; trial < 13; ++trial) {
    entry = valid_entry;
    switch (trial) {
      case 0: entry.current_el = 12; break;
      case 1: entry.daif = 0; break;
      case 2: entry.sctlr = 1; break;
      case 3: entry.sctlr = 4; break;
      case 4: entry.cntfrq = 0; break;
      case 5: entry.stack++; break;
      case 6: entry.x[0]++; break;
      case 7: entry.x[1] = 1; break;
      case 8: entry.x[2] = 1; break;
      case 9: entry.x[3] = 1; break;
      case 10: entry.resume = raw_kernel.base; break;
      case 11: entry.replay_mz = 2; break;
      case 12: entry.reserved = 1; break;
    }
    require(uke_sec_entry_admit(&entry, raw_kernel, raw_fdt) != UKE_OK, "unsupported SEC entry rejected");
  }
  require(uke_sec_entry_admit(&valid_entry, raw_kernel, {raw_fdt.base, 3 * 1024 * 1024}) != UKE_OK, "oversized FDT rejected");
  require(uke_sec_entry_admit(&valid_entry, {raw_fdt.base, 4096}, raw_fdt) != UKE_OK, "kernel/FDT collision rejected");
  require(uke_sec_entry_admit(nullptr, raw_kernel, raw_fdt) == UKE_INVALID_ARGUMENT, "absent captured context rejected");
  const std::array<UKE_RANGE, 2> ram {{{0x80000000, 0x80000000}, {0x100000000, 0x80000000}}};
  const std::array<UKE_RANGE, 2> reserved {{{0x80000000, 0x100000}, {0x9d400000, 0x4500000}}};
  UKE_MEMORY_INPUT m {ram.data(), ram.size(), reserved.data(), reserved.size(),
      {0x110000000, 0x10000000}, {0x90000000, 0x4000000}, {0x95000000, 0x100000},
      {0x96000000, 0x800000}, {0x93000000, 0x1000}, 1};
  UKE_MEMORY_PLAN plan {};
  require(uke_prepi_memory_plan(&m, 0x400000, 0x2000000, 0x40000, &plan) == UKE_OK, "reviewed nonoverlapping aperture");
  require(plan.firmware.base == m.allowed.base && plan.heap.base == plan.firmware.base + plan.firmware.bytes &&
      plan.stack.base == plan.heap.base + plan.heap.bytes, "contiguous layout");
  const auto original = m;
  for (unsigned trial = 0; trial < 12; ++trial) {
    m = original;
    switch (trial) {
      case 0: m.complete_ownership = 0; break;
      case 1: m.allowed = {0x9fc00000, 0x4000000}; break;
      case 2: m.allowed = m.kernel; break;
      case 3: m.allowed = m.fdt; break;
      case 4: m.allowed = m.initrd; break;
      case 5: m.allowed = m.entry_context; break;
      case 6: m.allowed = {UINT64_MAX - 10, 9}; break;
      case 7: m.allowed = {0x180000000, 0x4000000}; break;
      case 8: m.allowed.bytes = 4096; break;
      case 9: m.ram_count = 33; break;
      case 10: m.fdt = m.kernel; break;
      case 11: m.kernel.bytes = UINT64_MAX; break;
    }
    std::memset(&plan, 42, sizeof(plan));
    require(uke_prepi_memory_plan(&m, 0x400000, 0x2000000, 0x40000, &plan) != UKE_OK, "unsafe plan rejected");
    require(plan.firmware.bytes == 0 && plan.heap.bytes == 0 && plan.stack.bytes == 0, "failed plan has no published allocation");
  }
  m = original;
  require(uke_prepi_memory_plan(&m, UINT64_MAX - 4095, 4096, 4096, &plan) != UKE_OK, "sum overflow");
  require(uke_prepi_memory_plan(&m, 4097, 4096, 4096, &plan) != UKE_OK, "unaligned extent");
  require(!uke_range_valid({UINT64_MAX, 1}) && !uke_range_valid({1, 0}), "invalid half-open ranges");
  require(!uke_range_overlap({0, 4096}, {4096, 4096}), "adjacent boundaries do not overlap");
  for (unsigned mode = 0; mode < 5; ++mode) {
    Backend b;
    std::array<Backend::Driver, 3> drivers {{{&b, 0}, {&b, 1}, {&b, 2}}};
    UKE_RETURN_TRANSACTION t {};
    for (auto& driver : drivers) {
      size_t index = 99;
      require(uke_return_register(&t, &driver, Backend::quiesce, &index) == UKE_OK, "register before touching hardware");
      require(uke_return_mark_dirty(&t, index) == UKE_OK, "mark before DMA mutation");
    }
    if (mode == 1) b.admitted = false;
    if (mode == 2) b.fail_driver = 1;
    if (mode == 3) b.prepare_status = UKE_HASH_MISMATCH;
    if (mode == 4) b.exit_status = UKE_IO_ERROR;
    UKE_RETURN_BACKEND backend {&b, Backend::ready, Backend::prepare, Backend::exit, Backend::jump};
    require(uke_return_android(&t, &backend) != UKE_OK && t.stage == UKE_RETURN_FAILED, "returned/failed handoff is never success");
    if (mode == 1) require(b.count == 0, "unready backend does not touch drivers");
    else require(b.count >= 3 && b.order[0] == 2 && b.order[1] == 1 && b.order[2] == 0, "all dirty drivers quiesced in reverse order");
    const size_t expected = mode == 0 ? 6 : mode == 1 ? 0 : mode == 2 ? 3 : mode == 3 ? 4 : 5;
    require(b.count == expected, "later stages stop after first transition failure");
    require(uke_return_android(&t, &backend) == UKE_INVALID_ARGUMENT && b.count == expected, "no reset-loop retry");
    require(uke_return_mark_dirty(&t, 0) == UKE_INVALID_ARGUMENT, "late driver mutation rejected");
    if (mode == 2) require(t.drivers[1].dirty && !t.drivers[0].dirty && !t.drivers[2].dirty, "uncertain driver state retained");
  }
  std::printf("platform memory/return checks=%u passed\n", checks);
}
