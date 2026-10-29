#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: build-efi-dispatcher.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde=$(realpath -- "$2")
output="$component/build/efi-dispatcher"
mkdir -p -- "$output"
flags=(--target=aarch64-pc-windows-msvc -std=c11 -DUKE_EDK2 -O2 -g0 -ffreestanding -fno-builtin
    -fno-stack-protector -fshort-wchar -Wall -Wextra -Werror
    "-I$component/include" "-I$component/src/platform/UkePlatformPkg/Include"
    "-I$mde/Include" "-I$mde/Include/AArch64")
for source in gpt policy dispatch sha256 fat32; do
  "$compiler/clang" "${flags[@]}" -c "$component/src/core/$source.c" -o "$output/$source.obj"
done
"$compiler/clang" "${flags[@]}" -c "$component/src/platform/UkePlatformPkg/BootDispatcher/BootDispatcher.c" -o "$output/entry.obj"
"$compiler/clang" "${flags[@]}" -c "$component/src/efi/FreestandingMemory.c" -o "$output/memory.obj"
"$compiler/lld-link" /machine:arm64 /subsystem:efi_application /entry:UkeBootDispatchEntry \
    /nodefaultlib /timestamp:0 /out:"$output/UkeBootDispatcher.efi" "$output"/*.obj
"$compiler/llvm-readobj" --file-headers --coff-imports "$output/UkeBootDispatcher.efi" > "$output/pe-headers.txt"
sha256sum -- "$output/UkeBootDispatcher.efi"
printf 'EFI integration application, not raw firmware. Missing platform provider refuses dispatch.\n'
