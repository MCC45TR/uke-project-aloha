#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
[[ $# == 2 ]] || { printf 'Usage: build-efi-probe.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
clang_bin=$(realpath -- "$1")
mde_pkg=$(realpath -- "$2")
[[ -x $clang_bin/clang && -x $clang_bin/lld-link && -f $mde_pkg/Include/Uefi.h ]]
output="$component/build/efi-probe"
mkdir -p -- "$output"
flags=(--target=aarch64-pc-windows-msvc -std=c11 -DUKE_EDK2 -O2 -g0 -ffreestanding -fno-builtin
    -fno-stack-protector -fshort-wchar -Wall -Wextra -Werror
    "-I$component/include" "-I$mde_pkg/Include" "-I$mde_pkg/Include/AArch64"
    "-ffile-prefix-map=$component=/workspace/uke-project-aloha")
for source in gpt policy buttons sha256 fat32; do
  "$clang_bin/clang" "${flags[@]}" -H -c "$component/src/core/$source.c" -o "$output/$source.obj" 2> "$output/$source.includes"
done
"$clang_bin/clang" "${flags[@]}" -H -c "$component/src/efi/EspProbe.c" -o "$output/probe.obj" 2> "$output/probe.includes"
"$clang_bin/lld-link" /machine:arm64 /subsystem:efi_application /entry:UkeProbeEntry /nodefaultlib /timestamp:0 \
    /out:"$output/UkeEspProbe.efi" "$output/probe.obj" "$output/gpt.obj"
"$clang_bin/llvm-readobj" --file-headers --coff-imports "$output/UkeEspProbe.efi" > "$output/pe-headers.txt"
sha256sum "$output/UkeEspProbe.efi"
printf 'Built diagnostic EFI application; not a Uke bootable firmware image.\n'
