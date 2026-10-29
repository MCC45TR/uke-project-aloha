#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 3 || $# == 4 ]] || exit 2
tool=$(realpath -- "$1")
probe=$(realpath -- "$2")
boot=$(realpath -- "$3")
runner=()
if [[ $# == 4 ]]; then
  [[ $4 == qemu-aarch64 ]] || exit 2
  runner=(qemu-aarch64)
fi
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
boot_hash=$(sha256sum -- "$boot"); boot_hash=${boot_hash%% *}
probe_hash=$(sha256sum -- "$probe"); probe_hash=${probe_hash%% *}
"${runner[@]}" "$tool" stage-sec-probe "$boot" "$probe" "$boot_hash" "$probe_hash" "$work/staged" --unsigned-development
"${runner[@]}" "$tool" restore-sec-probe "$boot" "$probe" "$work/staged" "$boot_hash" "$probe_hash" "$work/restored"
cmp -- "$boot" "$work/restored"
reject() { if "${runner[@]}" "$tool" "$@" > "$work/stdout" 2> "$work/stderr"; then exit 1; fi; }
bad_hash=0000000000000000000000000000000000000000000000000000000000000000
reject stage-sec-probe "$boot" "$probe" "$bad_hash" "$probe_hash" "$work/wrong-boot" --unsigned-development
reject stage-sec-probe "$boot" "$probe" "$boot_hash" "$bad_hash" "$work/wrong-probe" --unsigned-development
reject stage-sec-probe "$boot" "$probe" "$boot_hash" "$probe_hash" "$work/staged" --unsigned-development
ln -s "$work/staged" "$work/symlink"
reject stage-sec-probe "$boot" "$probe" "$boot_hash" "$probe_hash" "$work/symlink" --unsigned-development
ln -s "$probe" "$work/payload-link"
reject stage-sec-probe "$boot" "$work/payload-link" "$boot_hash" "$probe_hash" "$work/link-out" --unsigned-development
mkfifo "$work/pipe"
reject stage-sec-probe "$boot" "$work/pipe" "$boot_hash" "$probe_hash" "$work/pipe-out" --unsigned-development
reject stage-sec-probe "$boot" "$probe" "$boot_hash" "$probe_hash" /dev/null --unsigned-development
reject restore-sec-probe "$boot" "$probe" "$boot" "$boot_hash" "$probe_hash" "$work/bad-restore"
[[ ! -e $work/wrong-boot && ! -e $work/wrong-probe && ! -e $work/link-out && ! -e $work/pipe-out && ! -e $work/bad-restore ]]
"${runner[@]}" "$tool" restore-sec-probe "$boot" "$probe" "$work/staged" "$boot_hash" "$probe_hash" "$work/protected-restore"
cmp -- "$boot" "$work/protected-restore"
printf 'SEC probe CLI: exact restore and 8 rejection scenarios passed\n'
