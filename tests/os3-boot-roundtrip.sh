#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 3 || $# == 4 ]] || { printf 'Usage: os3-boot-roundtrip.sh TOOL SELECTOR BOOT [qemu-aarch64]\n' >&2; exit 2; }
tool=$(realpath -- "$1")
selector=$(realpath -- "$2")
boot=$(realpath -- "$3")
runner=()
if [[ $# == 4 ]]; then
  [[ $4 == qemu-aarch64 ]] || exit 2
  runner=(qemu-aarch64)
fi
expected=efdee1d4e1acd7f6e77615330dbcb045caeeccad8568d89fa6abd627f606e77f
[[ $(sha256sum -- "$boot" | cut -d ' ' -f1) == "$expected" ]]
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
"${runner[@]}" "$tool" inspect "$boot"
"${runner[@]}" "$tool" stage-android-only "$boot" "$selector" "$expected" "$work/staged.img" --unsigned-development
[[ $(stat -c %s -- "$work/staged.img") == 100663296 ]]
"${runner[@]}" "$tool" restore-android-only "$boot" "$selector" "$work/staged.img" "$expected" "$work/restored.img"
cmp -- "$boot" "$work/restored.img"
reject() {
  local status=0
  "${runner[@]}" "$tool" "$@" > "$work/stdout" 2> "$work/stderr" || status=$?
  [[ $status != 0 ]]
}
reject stage-android-only "$boot" "$selector" "$expected" "$work/staged.img" --unsigned-development
ln -s "$work/staged.img" "$work/link.img"
before=$(sha256sum -- "$work/staged.img")
reject stage-android-only "$boot" "$selector" "$expected" "$work/link.img" --unsigned-development
[[ $(sha256sum -- "$work/staged.img") == "$before" ]]
reject stage-android-only "$boot" "$selector" "${expected/ef/de}" "$work/bad.img" --unsigned-development
[[ ! -e $work/bad.img ]]
reject stage-android-only "$boot" "$selector" "$expected" "$work/bad.img" --incorrect-mode
[[ ! -e $work/bad.img ]]
printf '\001' | dd of="$work/staged.img" bs=1 seek=5000 count=1 conv=notrunc status=none
reject restore-android-only "$boot" "$selector" "$work/staged.img" "$expected" "$work/bad.img"
[[ ! -e $work/bad.img ]]
[[ $(sha256sum -- "$boot" | cut -d ' ' -f1) == "$expected" ]]
printf 'OS3 regular-file round trip and rejection scenarios: 8/8 passed\n'
