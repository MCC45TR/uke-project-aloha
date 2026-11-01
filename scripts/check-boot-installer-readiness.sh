#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Read-only readiness report. Does not package, patch or access a device.
set -euo pipefail
[[ $# == 0 ]] || { printf 'Usage: check-boot-installer-readiness.sh\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
manifest="$component/manifests/boot-installer-readiness.json"
jq -e '
  .schema_version == 1 and .delivery_kind == "android-kernel-dualboot-installer-zip" and
  .target == "uke" and (.profile_id | type == "string") and
  (.installable_zip_built | type == "boolean") and
  (.requirements | type == "array" and length == 6) and
  ([.requirements[].id] | sort == ["arm64-patcher-and-boot-repacker",
    "esp-independent-android-return", "installer-backup-and-write-backend",
    "profile-matched-stock-boot", "uke-kernel-entry-selector", "uke-raw-firmware-payload"]) and
  all(.requirements[]; (.ready | type == "boolean") and (.reason | type == "string"))
' "$manifest" >/dev/null || { printf 'Invalid readiness manifest\n' >&2; exit 2; }
if jq -e 'any(.requirements[]; .ready == false)' "$manifest" >/dev/null; then
  printf 'boot-installer-zip=unavailable\n'
  jq -r '.requirements[] | select(.ready == false) | "missing=\(.id): \(.reason)"' "$manifest"
  exit 3
fi
printf 'prerequisite-record=complete\n'
printf 'An artifact/input audit is still required; this checker builds no ZIP.\n'
