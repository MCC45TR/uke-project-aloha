#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: check-efi-dispatcher-host.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde=$(realpath -- "$2")
output="$component/build/efi-dispatcher-host"
mkdir -p -- "$output"
flags=(-std=c11 -DUKE_EDK2 -O1 -g0 -fno-builtin -fshort-wchar -Wall -Wextra -Werror -Wno-incompatible-library-redeclaration
    -fsanitize=address,undefined -fno-omit-frame-pointer
    "-I$component/include" "-I$component/src/platform/UkePlatformPkg/Include"
    "-I$mde/Include" "-I$mde/Include/AArch64")
for source in gpt policy dispatch sha256 fat32; do
  "$compiler/clang" "${flags[@]}" -c "$component/src/core/$source.c" -o "$output/$source.o"
done
"$compiler/clang" "${flags[@]}" "$component/tests/efi_dispatch_tests.c" "$output"/*.o -o "$output/efi-dispatch-tests"
ASAN_OPTIONS=detect_leaks=1 "$output/efi-dispatch-tests"
