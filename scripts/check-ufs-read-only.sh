#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: check-ufs-read-only.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde=$(realpath -- "$2")
output="$component/build/ufs-read-only"
mkdir -p -- "$output"
flags=(-std=c11 -DUKE_EDK2 -O1 -g0 -fno-builtin -fshort-wchar -Wall -Wextra -Werror
    "-I$component/include" "-I$component/src/platform/UkePlatformPkg/Include"
    "-I$mde/Include" "-I$mde/Include/AArch64")
sources=("$component/src/platform/UkePlatformPkg/UfsReadOnly/UfsReadOnly.c" "$component/src/core/sha256.c")
"$compiler/clang" "${flags[@]}" -Wno-incompatible-library-redeclaration -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$component/tests/ufs_read_only_tests.c" "${sources[@]}" -o "$output/ufs-host-tests"
ASAN_OPTIONS=detect_leaks=1 "$output/ufs-host-tests"
for source in "${sources[@]}"; do
  name=$(basename -- "${source%.c}")
  "$compiler/clang" --target=aarch64-pc-windows-msvc "${flags[@]}" -O2 -c "$source" -o "$output/$name.obj"
done
"$compiler/llvm-ar" rcs "$output/UkeUfsReadOnly.lib" "$output/UfsReadOnly.obj" "$output/sha256.obj"
"$compiler/llvm-readobj" --file-headers "$output/UfsReadOnly.obj" > "$output/coff-headers.txt"
printf 'UFS adapter compiled as ARM64 COFF library; not an initializing SM7675 DXE.\n'
