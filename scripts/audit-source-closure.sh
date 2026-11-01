#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Read-only checkout identity audit. Does not acquire sources or promote archive status.
set -euo pipefail
[[ $# == 0 ]] || exit 2
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output="$component/build/source-closure"
mkdir -p -- "$output"
records="$output/nodes.jsonl"
: > "$records"
nodes=0
audit() {
  local path=$1 expected=$2 name=$3 depth=$4 head status mode type sha child top
  ((++nodes))
  if ((nodes>128 || depth>8)); then printf 'Dependency audit bound exceeded\n' >&2; return 1; fi
  head=
  top=$(git -C "$path" rev-parse --show-toplevel 2>/dev/null || true)
  status=exact-checkout
  if [[ ! -d $path || $top != "$(realpath -m -- "$path")" ]]; then status=missing-checkout
  else head=$(git -C "$path" rev-parse HEAD 2>/dev/null || true)
  fi
  if [[ $status == missing-checkout ]]; then :
  elif [[ $head != "$expected" ]]; then status=commit-mismatch
  fi
  jq -nc --arg path "$name" --arg expected "$expected" --arg observed "$head" --arg status "$status" \
    '{path:$path,expected_commit:$expected,observed_commit:$observed,status:$status}' >> "$records"
  [[ $status == exact-checkout ]] || return 0
  while read -r mode type sha child; do
    [[ $mode == 160000 && $type == commit ]] || continue
    [[ $child != /* && $child != *..* && $child != *$'\n'* ]] || return 1
    audit "$path/$child" "$sha" "$name/$child" "$((depth+1))"
  done < <(git -C "$path" ls-tree -r "$expected")
}
expected=$(jq -er '.sources[]|select(.id=="aloha-platforms")|.refs["refs/heads/main"]' "$component/manifests/source-plan.json")
audit "$component/referances/donors/aloha-platforms" "$expected" referances/donors/aloha-platforms 0
jq -s '{schema_version:1,scope:"read-only-exact-checkout-identity",nodes:.,
    exact:(map(select(.status=="exact-checkout"))|length),
    unresolved:(map(select(.status!="exact-checkout"))|length),
    archive_completeness_established:false,licenses_reviewed:false,build_reproduced:false}' "$records" > "$output/audit.json"
jq '{exact,unresolved,archive_completeness_established}' "$output/audit.json"
