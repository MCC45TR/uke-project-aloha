#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Records the narrow standalone diagnostic build, never platform admission.
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: record-build-evidence.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde_pkg=$(realpath -- "$2")
output="$component/build/efi-probe"
[[ -s $output/UkeEspProbe.efi && -s $output/probe.includes ]]
records="$output/inputs.jsonl"
: > "$records"
record() {
  local path=$1 name=$2 kind=$3 digest
  digest=$(sha256sum -- "$path"); digest=${digest%% *}
  jq -nc --arg path "$name" --arg sha "$digest" --arg kind "$kind" \
      '{path:$path,sha256:$sha,kind:$kind}' >> "$records"
}
for name in gpt policy buttons sha256 fat32; do record "$component/src/core/$name.c" "src/core/$name.c" project-source; done
record "$component/src/efi/EspProbe.c" src/efi/EspProbe.c project-source
record "$component/scripts/build-efi-probe.sh" scripts/build-efi-probe.sh build-script
record "$component/src/efi/UkeEspProbe.inf" src/efi/UkeEspProbe.inf integration-metadata
record "$component/UkeBootCorePkg.dec" UkeBootCorePkg.dec integration-metadata
while IFS= read -r path; do
  if [[ $path == "$component/include/"* ]]; then
    record "$path" "${path#"$component/"}" project-header
  elif [[ $path == "$mde_pkg/"* ]]; then
    record "$path" "MdePkg/${path#"$mde_pkg/"}" upstream-header
  else printf 'Unclassified compiler input; refusing public record\n' >&2; exit 1
  fi
done < <(sed -n 's/^\.[.]* //p' "$output"/*.includes | LC_ALL=C sort -u)
for tool in clang lld-link; do record "$compiler/$tool" "clang-r547379/bin/$tool" host-build-tool; done
record "$output/UkeEspProbe.efi" build/efi-probe/UkeEspProbe.efi diagnostic-artifact
mu_commit=$(git -C "$mde_pkg" rev-parse HEAD)
jq -s --arg date "$(date -u +%Y-%m-%d)" --arg mu "$mu_commit" \
    '{schema_version:1,date_utc:$date,scope:"standalone-aarch64-efi-diagnostic-only",
      mu_basecore_commit:$mu,compiler_family:"Clang 20 clang-r547379",
      inputs:.,reproducible_platform_build:false,bootable_uke_firmware:false,
      full_source_archive_complete:false,hardware_executed:false}' \
    "$records" > "$output/build-inputs.lock.json"
printf 'Recorded standalone build inputs under build/efi-probe.\n'
