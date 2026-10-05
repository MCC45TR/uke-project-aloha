#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Standalone host/ARM64 EFI diagnostic build, with one compiler job at a time.
set -Eeuo pipefail
component=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# == 2 ]] || { echo 'Usage: build-linux-handoff.sh CLANG_BIN_DIR MDEPKG_DIRECTORY' >&2; exit 2; }
clang_bin=$(realpath "$1") mde=$(realpath "$2") out=$component/build/linux-handoff
[[ -x $clang_bin/clang && -x $clang_bin/lld-link && -f $mde/Include/Uefi.h ]]
mkdir -p "$out"
cc -std=c11 -O2 -Wall -Wextra -Werror -Wconversion -Wshadow -I"$component/include" \
    -c "$component/src/linux/memory_map.c" -o "$out/memory-host.o"
c++ -std=c++20 -O2 -Wall -Wextra -Werror -Wconversion -Wshadow -I"$component/include" \
    "$component/tests/linux-memory-map.cpp" "$out/memory-host.o" -o "$out/memory-fixtures"
"$out/memory-fixtures" | tee "$out/host-tests.txt"
flags=(--target=aarch64-pc-windows-msvc -std=c11 -DUKE_EDK2 -O2 -g0 -ffreestanding -fno-builtin
    -fno-stack-protector -fshort-wchar -Wall -Wextra -Werror
    "-I$component/include" "-I$mde/Include" "-I$mde/Include/AArch64"
    "-ffile-prefix-map=$component=/workspace/uke-project-aloha")
"$clang_bin/clang" "${flags[@]}" -H -c "$component/src/linux/memory_map.c" -o "$out/memory.obj" 2> "$out/memory.includes"
"$clang_bin/clang" "${flags[@]}" -H -c "$component/src/efi/LinuxHandoffProbe.c" -o "$out/probe.obj" 2> "$out/probe.includes"
"$clang_bin/lld-link" /machine:arm64 /subsystem:efi_application /entry:UkeLinuxHandoffEntry /nodefaultlib /timestamp:0 \
    /out:"$out/UkeLinuxHandoffProbe.efi" "$out/probe.obj" "$out/memory.obj"
"$clang_bin/llvm-readobj" --file-headers --coff-imports "$out/UkeLinuxHandoffProbe.efi" > "$out/pe-headers.txt"
sha256sum "$out/UkeLinuxHandoffProbe.efi"
echo 'ARM64 EFI diagnostic compiled; Uke firmware backend and final Linux handoff remain unverified.'
