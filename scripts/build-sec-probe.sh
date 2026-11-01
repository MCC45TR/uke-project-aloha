#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: build-sec-probe.sh CLANG_BIN_DIR AARCH64_MUSL_SYSROOT\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
sysroot=$(realpath -- "$2")
libfdt="$component/src/upstream/dtc-7a1e017926004ecff5fce62d62d42ce9f3e00082/libfdt"
[[ -f $libfdt/libfdt.h && -x $compiler/clang && -d $sysroot/include ]]
output="$component/build/sec-probe"
mkdir -p -- "$output"
# MMU-off Device memory faults on compiler-merged unaligned FDT loads.
flags=(--target=aarch64-unknown-linux-musl "--sysroot=$sysroot" -D_POSIX_C_SOURCE=200809L -O2 -g0 -ffreestanding
  -fno-builtin -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
  -fpie -mgeneral-regs-only -mstrict-align -mno-outline-atomics -ffunction-sections -fdata-sections
  "-ffile-prefix-map=$component=/workspace/uke-project-aloha" "-I$component/include" "-I$libfdt")
objects=()
for source in "$component/src/platform/UkePlatformPkg/Sec/"{ProbeMain,ProbeMemory,FramebufferAdmission}.c \
  "$component/src/efi/FreestandingMemory.c" "$libfdt/"{fdt,fdt_ro,fdt_check,fdt_addresses}.c; do
  object="$output/$(basename -- "${source%.c}").o"
  "$compiler/clang" "${flags[@]}" -std=c11 -Wall -Wextra -Werror -c "$source" -o "$object"
  objects+=("$object")
done
"$compiler/clang" "${flags[@]}" -c "$component/src/platform/UkePlatformPkg/Sec/ProbeEntry.S" -o "$output/ProbeEntry.o"
"$compiler/ld.lld" -static --gc-sections -T "$component/src/platform/UkePlatformPkg/Sec/Probe.lds" \
  "$output/ProbeEntry.o" "${objects[@]}" -o "$output/UkeAlohaSec.elf"
"$compiler/llvm-objcopy" -O binary "$output/UkeAlohaSec.elf" "$output/UkeAlohaSec.bin"
"$compiler/llvm-readobj" --file-headers --relocations --needed-libs "$output/UkeAlohaSec.elf" > "$output/elf-headers.txt"
"$compiler/llvm-objdump" -d "$output/UkeAlohaSec.elf" > "$output/disassembly.txt"
[[ $(stat -c %s -- "$output/UkeAlohaSec.bin") -le 524288 ]]
[[ -z $("$compiler/llvm-nm" --undefined-only "$output/UkeAlohaSec.elf") ]]
! rg -q 'R_AARCH64|NEEDED|INTERP' "$output/elf-headers.txt"
sha256sum -- "$output/UkeAlohaSec.bin"
printf 'Built raw Uke SEC entry probe; no DXE, storage driver or boot manager.\n'
