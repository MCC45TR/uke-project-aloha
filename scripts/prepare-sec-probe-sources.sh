#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest="$component/manifests/sec-probe-sources-2026-10-12.json"
mkdir -p -- "$component/referances/snapshots" "$component/src/upstream" "$component/build"
work=$(mktemp -d "$component/build/source-import.XXXXXX")
trap 'rm -rf -- "$work"' EXIT
while IFS=$'\t' read -r root url archive expected; do
  [[ $root =~ ^[a-zA-Z0-9_-]+$ && $archive =~ ^[a-zA-Z0-9_.-]+$ && $expected =~ ^[a-f0-9]{64}$ ]]
  cached="$component/referances/snapshots/$archive"
  if [[ ! -f $cached ]]; then
    curl -fL --retry 2 "$url" -o "$work/$archive"
    [[ $(sha256sum -- "$work/$archive") == "$expected "* ]]
    mv -n -- "$work/$archive" "$cached"
  fi
  [[ $(sha256sum -- "$cached") == "$expected "* ]]
  tar -tzf "$cached" > "$work/entries"
  ! rg -v "^$root(/|$)" "$work/entries"
  ! rg '(^|/)\.\.(/|$)' "$work/entries"
  tar --extract --gzip --file "$cached" --directory "$work" --no-same-owner --no-same-permissions
  if [[ -e $component/src/upstream/$root ]]; then
    diff -qr -- "$work/$root" "$component/src/upstream/$root"
  else
    mv -n -- "$work/$root" "$component/src/upstream/$root"
  fi
  printf 'Verified source import: %s\n' "$root"
done < <(jq -r '.imports[] | [.root,.url,.archive,.sha256] | @tsv' "$manifest")
