#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 1 ]] || { printf 'Usage: check-arm64-core.sh CLANG_BIN_DIR\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
output="$component/build/arm64-fixture"
mkdir -p -- "$output"
command -v qemu-aarch64 >/dev/null
flags=(--target=aarch64-linux-gnu -O2 -g0 -ffreestanding -fno-builtin -fno-stack-protector
    -Wall -Wextra -Werror "-I$component/include")
for source in gpt policy buttons sha256 fat32; do
  "$compiler/clang" "${flags[@]}" -std=c11 -c "$component/src/core/$source.c" -o "$output/$source.o"
done
"$compiler/clang" "${flags[@]}" -std=c11 -c "$component/src/efi/FreestandingMemory.c" -o "$output/memory.o"
"$compiler/clang++" "${flags[@]}" -std=c++20 -fno-exceptions -fno-rtti -c "$component/tests/arm64_smoke.cpp" -o "$output/smoke.o"
"$compiler/clang++" --target=aarch64-linux-gnu -nostdlib -static -fuse-ld=lld \
    -Wl,-e,_start -Wl,--build-id=sha1 "$output"/*.o -o "$output/uke-core-fixture"
qemu-aarch64 "$output/uke-core-fixture" | tee "$output/result.txt"
"$compiler/llvm-readobj" --file-headers "$output/uke-core-fixture" > "$output/elf-headers.txt"
