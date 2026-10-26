#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 1 ]] || { printf 'Usage: build-android-continuation.sh CLANG_BIN_DIR\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
output="$component/build/android-continuation"
mkdir -p -- "$output"
"$compiler/clang" --target=aarch64-none-elf -c "$component/src/boot/AndroidContinuation.S" -o "$output/selector.o"
"$compiler/ld.lld" -Ttext=0 --oformat=binary -e _start "$output/selector.o" -o "$output/AndroidContinuation.bin"
[[ $(stat -c %s -- "$output/AndroidContinuation.bin") == 256 ]]
"$compiler/llvm-objdump" -d "$output/selector.o" > "$output/disassembly.txt"
sha256sum -- "$output/AndroidContinuation.bin"
