#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: check-efi-return.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde=$(realpath -- "$2")
output="$component/build/efi-return"
mkdir -p -- "$output"
flags=(-std=c11 -DUKE_EDK2 -O1 -g0 -fno-builtin -fshort-wchar -Wall -Wextra -Werror
    "-I$component/include" "-I$component/src/platform/UkePlatformPkg/Include"
    "-I$mde/Include" "-I$mde/Include/AArch64")
"$compiler/clang" "${flags[@]}" -Wno-incompatible-library-redeclaration -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$component/tests/efi_return_tests.c" "$component/src/platform/UkePlatformPkg/AndroidReturn/ExitServices.c" \
    -o "$output/efi-return-host-tests"
ASAN_OPTIONS=detect_leaks=1 "$output/efi-return-host-tests"
for source in ExitServices ReturnTransaction; do
  "$compiler/clang" --target=aarch64-pc-windows-msvc "${flags[@]}" -O2 \
      -c "$component/src/platform/UkePlatformPkg/AndroidReturn/$source.c" -o "$output/$source.obj"
done
"$compiler/llvm-ar" rcs "$output/UkeAndroidReturn.lib" "$output/ExitServices.obj" "$output/ReturnTransaction.obj"
"$compiler/llvm-readobj" --file-headers "$output/ExitServices.obj" > "$output/coff-headers.txt"
printf 'EFI exit/return libraries compiled as ARM64 COFF; real board restoration remains required.\n'
