/* SPDX-License-Identifier: MIT */
#include "uke/linux_handoff.h"
#include <array>
#include <cstdint>
#include <cassert>
#include <iostream>
static void put(std::uint8_t* p, std::uint64_t v, unsigned n) {
  for (unsigned i=0; i<n; ++i) p[i] = static_cast<std::uint8_t>(v >> (i*8));
}
int main() {
  std::array<std::uint8_t, 96> map{};
  put(map.data(), 7, 4); put(map.data()+8, 0x100000, 8); put(map.data()+24, 16, 8);
  put(map.data()+48, 6, 4); put(map.data()+56, 0x200000, 8);
  put(map.data()+72, 2, 8); put(map.data()+80, 1ULL<<63, 8);
  UKE_MEMORY_INVENTORY result{};
  const auto baseline=map;
  assert(uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  assert(result.descriptor_count==2 && result.conventional_bytes==65536 && result.runtime_descriptors==1);
  assert(map==baseline);
  assert(!uke_memory_map_check(map.data(), map.size()-1, 48, 1, &result));
  assert(result.descriptor_count==0);
  assert(!uke_memory_map_check(map.data(), map.size(), 39, 1, &result));
  assert(!uke_memory_map_check(map.data(), map.size(), 48, 2, &result));
  put(map.data()+56, 0x100000, 8); assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  map=baseline; put(map.data()+8, 0x100001, 8); assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  map=baseline; put(map.data()+24, UINT64_MAX, 8); assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  map=baseline; put(map.data()+8, UINT64_MAX-4095, 8); put(map.data()+24, 2, 8);
  assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  map=baseline; put(map.data()+24, 0, 8); assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  map=baseline; put(map.data(), 16, 4); assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  map=baseline; put(map.data()+56, 0x110000, 8); // Adjacent extents are valid.
  assert(uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  std::array<std::uint8_t, 48*2049> excessive{};
  assert(!uke_memory_map_check(excessive.data(), excessive.size(), 48, 1, &result));
  map=baseline; put(map.data(), 6, 4); assert(!uke_memory_map_check(map.data(), map.size(), 48, 1, &result));
  assert(!uke_memory_map_check(nullptr, 0, 48, 1, &result));
  assert(!uke_memory_map_check(baseline.data(), baseline.size(), 48, 1, nullptr));
  std::cout << "EFI memory ABI, RAM count, read-only, overlap/adjacency, bounds, stride, version and overflow fixtures passed\n";
}
