#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: check-sec-entry.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde=$(realpath -- "$2")
output="$component/build/sec-entry"
mkdir -p -- "$output"
flags=(-DUKE_EDK2 -O2 -g0 -fno-builtin -fshort-wchar -Wall -Wextra -Werror
    "-I$component/include" "-I$mde/Include" "-I$mde/Include/AArch64")
for source in Sec/EntryAdmission.c Sec/CaptureEntry.S PrePi/MemoryPlan.c AndroidReturn/ResumeRaw.S; do
  name=$(basename -- "$source")
  "$compiler/clang" --target=aarch64-pc-windows-msvc "${flags[@]}" \
    -c "$component/src/platform/UkePlatformPkg/$source" -o "$output/${name%.*}.obj"
done
"$compiler/llvm-ar" rcs "$output/UkeSecEntry.lib" "$output/EntryAdmission.obj" "$output/MemoryPlan.obj" \
  "$output/CaptureEntry.obj" "$output/ResumeRaw.obj"
command -v qemu-aarch64 >/dev/null
for mode in branch mz; do
  entry_flags=()
  [[ $mode != mz ]] || entry_flags=(-DMZ_ENTRY=1)
  "$compiler/clang" --target=aarch64-linux-gnu -nostdlib -static -fuse-ld=lld -Wl,-e,_start \
    "${entry_flags[@]}" "$component/tests/raw_resume_smoke.S" \
    "$component/src/platform/UkePlatformPkg/AndroidReturn/ResumeRaw.S" -o "$output/fixture-$mode"
  qemu-aarch64 "$output/fixture-$mode"
  printf 'Raw resume (%s): QEMU-user x0-x3/SP restoration and original branch checks passed\n' "$mode"
done
printf 'SEC capture compiled only; privileged capture and physical restoration not executed.\n'
