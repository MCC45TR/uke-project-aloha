#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 3 ]] || exit 2
inspector=$(realpath -- "$1")
fixture_builder=$(realpath -- "$2")
efi=$(realpath -- "$3")
[[ -f $efi && ! -L $efi ]]
for tool in mkfs.fat mmd mcopy; do command -v "$tool" >/dev/null; done
work=$(mktemp -d)
cleanup() {
  for file in "$work"/*; do [[ -f $file && ! -L $file ]] && unlink "$file"; done
  rmdir "$work"
}
trap cleanup EXIT
truncate -s 64M "$work/fat.img"
[[ -f $work/fat.img && ! -L $work/fat.img ]]
mkfs.fat -F 32 -S 512 -s 1 "$work/fat.img" > "$work/mkfs.log"
mmd -i "$work/fat.img" ::/EFI ::/EFI/UKE
digest=$(sha256sum "$efi"); digest=${digest%% *}
make_config() {
  printf 'schema=1\nprofile=%s\nmanager=%s\nsha256=%s\n' "$1" "$2" "$digest" > "$work/BOOT.CFG"
  mcopy -o -i "$work/fat.img" "$work/BOOT.CFG" ::/EFI/UKE/BOOT.CFG
}
make_config uke-fixture refind
mcopy -i "$work/fat.img" "$efi" ::/EFI/UKE/REFIND.EFI
check_image() {
  local id=$1 mode=$2 expected=$3
  "$fixture_builder" "$work/fat.img" "$work/$id.img" "$mode"
  local before
  before=$(sha256sum "$work/$id.img")
  "$inspector" boot "$work/$id.img" 512 uke-fixture > "$work/$id.result"
  rg -q -x "$expected" "$work/$id.result"
  [[ $(sha256sum "$work/$id.img") == "$before" ]]
}
check_image refind one boot-policy=manager-eligible
check_image absent none reason=no-esp
check_image duplicate two reason=ambiguous-esp
make_config another-profile refind
check_image profile one reason=wrong-profile
make_config uke-fixture systemd-boot
check_image missing one reason=file-missing
mcopy -i "$work/fat.img" "$efi" ::/EFI/UKE/SDBOOT.EFI
check_image systemd one boot-policy=manager-eligible
make_config uke-fixture refind
digest=$(printf '%064d' 0)
make_config uke-fixture refind
check_image corrupt_hash one reason=hash-mismatch
printf '\000' | dd of="$work/fat.img" bs=1 seek=510 conv=notrunc status=none
check_image corrupt_fat one reason=invalid-filesystem
printf '\000' | dd of="$work/refind.img" bs=1 seek=512 conv=notrunc status=none
bad_gpt_before=$(sha256sum "$work/refind.img")
"$inspector" boot "$work/refind.img" 512 uke-fixture > "$work/bad-gpt.result"
rg -q -x reason=invalid-gpt "$work/bad-gpt.result"
[[ $(sha256sum "$work/refind.img") == "$bad_gpt_before" ]]
printf 'Real FAT32/GPT file round trips: 9 cases passed; no manager or OS executed.\n'
