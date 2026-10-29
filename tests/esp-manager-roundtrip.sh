#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 3 || $# == 4 ]] || exit 2
inspector=$(realpath -- "$1")
builder=$(realpath -- "$2")
efi=$(realpath -- "$3")
dispatch=${4:-}
[[ -z $dispatch ]] || dispatch=$(realpath -- "$dispatch")
for tool in mkfs.fat mmd mcopy mdel; do command -v "$tool" > /dev/null; done
work=$(mktemp -d)
cleanup() {
  for file in "$work"/*; do [[ -f $file && ! -L $file ]] && unlink -- "$file"; done
  rmdir -- "$work"
}
trap cleanup EXIT
truncate -s 64M "$work/fat.img"
mkfs.fat -F 32 -S 512 -s 1 "$work/fat.img" > "$work/mkfs.log"
mmd -i "$work/fat.img" ::/EFI ::/EFI/refind ::/EFI/systemd ::/EFI/BOOT
checks=0
check_image() {
  local id=$1 mode=$2 expected=$3 path=${4:-} before selected
  "$builder" "$work/fat.img" "$work/$id.img" "$mode"
  before=$(sha256sum -- "$work/$id.img")
  "$inspector" boot "$work/$id.img" 512 uke-fixture > "$work/$id.result"
  rg -Fxq -- "$expected" "$work/$id.result"
  [[ -z $path ]] || rg -Fxiq "owned-esp-path=$path" "$work/$id.result"
  if [[ -n $dispatch && -n $path ]]; then
    selected=$(sed -n 's/^owned-esp-path=//p' "$work/$id.result")
    "$dispatch" "$work/$id.img" "$selected"
  fi
  [[ $(sha256sum -- "$work/$id.img") == "$before" ]]
  checks=$((checks+1))
}
check_image empty one reason=file-missing
mcopy -i "$work/fat.img" "$efi" ::/EFI/refind/refind_aa64.efi
check_image refind one boot-policy=manager-eligible EFI/refind/refind_aa64.efi
check_image unnamed unnamed reason=no-esp
check_image no_esp none reason=no-esp
check_image two_esps two reason=ambiguous-esp
mcopy -i "$work/fat.img" "$efi" ::/EFI/systemd/systemd-bootaa64.efi
check_image two_managers one reason=ambiguous-manager
mcopy -i "$work/fat.img" "$efi" ::/EFI/BOOT/BOOTAA64.EFI
check_image preferred one boot-policy=manager-eligible EFI/BOOT/BOOTAA64.EFI
cp -- "$efi" "$work/wrong.efi"
pe_offset=$(od -An -tu4 -j60 -N4 "$work/wrong.efi")
printf '\144\206' | dd of="$work/wrong.efi" bs=1 seek=$((pe_offset+4)) conv=notrunc status=none
mcopy -o -i "$work/fat.img" "$work/wrong.efi" ::/EFI/BOOT/BOOTAA64.EFI
check_image invalid_default one reason=invalid-image
mdel -i "$work/fat.img" ::/EFI/BOOT/BOOTAA64.EFI ::/EFI/refind/refind_aa64.efi
check_image systemd one boot-policy=manager-eligible EFI/systemd/systemd-bootaa64.efi
mcopy -i "$work/fat.img" "$work/wrong.efi" ::/EFI/refind/refind_aa64.efi
check_image wrong_arch_sibling one boot-policy=manager-eligible EFI/systemd/systemd-bootaa64.efi
mdel -i "$work/fat.img" ::/EFI/systemd/systemd-bootaa64.efi ::/EFI/refind/refind_aa64.efi
mmd -i "$work/fat.img" ::/EFI/Custom
mcopy -i "$work/fat.img" "$efi" ::/EFI/Custom/another64.efi
check_image other_manager one boot-policy=manager-eligible EFI/Custom/another64.efi
printf 'Automatic ESP manager FAT32/GPT file scenarios: %s/11 passed; no EFI execution\n' "$checks"
