#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 3 ]] || { printf 'Usage: make-sec-test-image.sh {fedora-0242|os3-global} INPUT_BOOT OUTPUT_IMG\n' >&2; exit 2; }
bundle=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
[[ $(uname -m) == x86_64 ]] || { printf 'This bundle requires an x86_64 Linux host.\n' >&2; exit 2; }
# Keep caller-relative file names while running the checksum check in the bundle.
input=$2
output=$3
[[ $input == /* ]] || input="$PWD/$input"
[[ $output == /* ]] || output="$PWD/$output"
cd -- "$bundle"
sha256sum --check --strict SHA256SUMS > /dev/null
profile=$1
[[ $profile == fedora-0242 || $profile == os3-global ]]
expected=$(jq -er --arg profile "$profile" '.profiles[$profile].sha256' profile.json)
probe_hash=$(jq -er '.probe_sha256' profile.json)
parent=$(dirname -- "$output")
[[ -d $parent && ! -e $output && ! -L $output ]]
work=$(mktemp -d "$parent/.uke-sec-probe.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
./uke-boot-tool stage-sec-probe "$input" UkeAlohaSec.bin "$expected" "$probe_hash" "$work/staged.img" --unsigned-development
./uke-boot-tool restore-sec-probe "$input" UkeAlohaSec.bin "$work/staged.img" "$expected" "$probe_hash" "$work/restored.img"
cmp -- "$input" "$work/restored.img"
ln -T -- "$work/staged.img" "$output"
sync -f -- "$parent"
printf 'Created and verified RAM-test image: %s\nNo device or partition was accessed.\n' "$output"
