#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 1 ]] || exit 2
tool=$(realpath -- "$1")
work=$(mktemp -d)
trap 'rm -rf -- "$work"' EXIT
expect_two() {
  local status=0
  "$tool" inspect "$1" > "$work/stdout" 2> "$work/stderr" || status=$?
  [[ $status == 2 ]]
}
expect_two "$work/missing"
expect_two "$work"
mkfifo "$work/pipe"
expect_two "$work/pipe"
truncate -s 134217729 "$work/oversize"
expect_two "$work/oversize"
ln -s "$work/oversize" "$work/link"
expect_two "$work/link"
expect_two /dev/null
printf 'boot tool file boundaries: 6/6 passed\n'
