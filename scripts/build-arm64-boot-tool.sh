#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: build-arm64-boot-tool.sh CLANG_BIN_DIR AARCH64_MUSL_SYSROOT\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
sysroot=$(realpath -- "$2")
runtime=$(realpath -- "$compiler/../lib/aarch64-unknown-linux-musl")
includes=$(realpath -- "$compiler/../include")
output="$component/build/arm64-boot-tool"
mkdir -p -- "$output"
flags=(--target=aarch64-unknown-linux-musl "--sysroot=$sysroot" -O2 -g0
    -Wall -Wextra -Werror -Wconversion -Wshadow "-I$component/include")
"$compiler/clang" "${flags[@]}" -std=c11 -fno-builtin -c "$component/src/core/sha256.c" -o "$output/sha256.o"
for source in src/boot/boot_image.cpp src/host/boot_tool.cpp; do
  name=$(basename -- "${source%.cpp}")
  "$compiler/clang++" "${flags[@]}" -std=c++20 -fno-exceptions -fno-rtti -nostdinc++ \
      "-I$includes/aarch64-unknown-linux-musl/c++/v1" "-I$includes/c++/v1" \
      -c "$component/$source" -o "$output/$name.o"
done
"$compiler/clang++" "${flags[@]}" -std=c++20 -fno-exceptions -fno-rtti -nostdinc++ \
    "-I$includes/aarch64-unknown-linux-musl/c++/v1" "-I$includes/c++/v1" \
    -c "$component/tests/boot_image_tests.cpp" -o "$output/tests.o"
"$compiler/clang++" --target=aarch64-unknown-linux-musl "--sysroot=$sysroot" -static \
    -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind -nostdlib++ -Wl,--build-id=sha1 \
    "$output/tests.o" "$output/boot_image.o" "$output/sha256.o" "-L$runtime" -lc++ -lc++abi -lunwind \
    -o "$output/uke-boot-image-tests"
"$compiler/clang++" --target=aarch64-unknown-linux-musl "--sysroot=$sysroot" -static \
    -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind -nostdlib++ -Wl,--build-id=sha1 "$output/boot_tool.o" \
    "$output/boot_image.o" "$output/sha256.o" "-L$runtime" -lc++ -lc++abi -lunwind \
    -o "$output/uke-boot-tool"
"$compiler/llvm-strip" --strip-all --remove-section=.comment "$output/uke-boot-tool"
"$compiler/llvm-readobj" --file-headers --program-headers --needed-libs "$output/uke-boot-tool" > "$output/elf-headers.txt"
sha256sum -- "$output/uke-boot-tool"
