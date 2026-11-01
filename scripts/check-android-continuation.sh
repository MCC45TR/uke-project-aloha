#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 1 ]] || { printf 'Usage: check-android-continuation.sh CLANG_BIN_DIR\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
bash "$component/scripts/build-android-continuation.sh" "$compiler"
output="$component/build/android-continuation"
command -v qemu-aarch64 >/dev/null
cd -- "$output"
for mode in branch mz; do
  flags=()
  [[ $mode != mz ]] || flags=(-DMZ_ENTRY=1)
  "$compiler/clang" --target=aarch64-linux-gnu -nostdlib -static -fuse-ld=lld \
      -Wl,-e,_start "${flags[@]}" "$component/tests/android_continuation_smoke.S" -o "fixture-$mode"
  qemu-aarch64 "fixture-$mode"
  printf 'Android continuation (%s entry): QEMU-user x0-x3/SP checks passed\n' "$mode"
done
