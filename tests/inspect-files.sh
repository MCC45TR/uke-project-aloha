#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
inspector=${1:?inspector executable required}
fixture=$(mktemp -d)
cleanup() { unlink "$fixture/symlink"; unlink "$fixture/pipe"; unlink "$fixture/image"; rmdir "$fixture"; }
trap cleanup EXIT
truncate -s 128K "$fixture/image"
ln -s image "$fixture/symlink"
mkfifo "$fixture/pipe"
for rejected in "$fixture/symlink" "$fixture/pipe" "$fixture" /dev/null; do
  if timeout 5 "$inspector" gpt "$rejected" 512 > /dev/null 2>&1; then
    printf 'Non-regular or symlink input was accepted\n' >&2; exit 1
  else
    [[ $? == 2 ]] || { printf 'Wrong rejection status or blocked read\n' >&2; exit 1; }
  fi
done
before=$(sha256sum "$fixture/image")
"$inspector" gpt "$fixture/image" 512 > /dev/null && exit 1
[[ $(sha256sum "$fixture/image") == "$before" ]]
printf 'File-only inspection, bounded special-file refusal and unchanged input: passed\n'
