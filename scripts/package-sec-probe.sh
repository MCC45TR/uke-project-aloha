#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd -- "$component"
output=artifacts/uke-aloha-sec-probe-20261012
[[ ! -e $output.zip && ! -L $output.zip && ! -e $output.zip.sha256 && ! -L $output.zip.sha256 ]]
mkdir -p artifacts
mkdir -- "$output"
cp -- build/entry-20261012/uke-boot-tool "$output/uke-boot-tool"
cp -- build/sec-probe/UkeAlohaSec.bin "$output/UkeAlohaSec.bin"
cp -- profiles/sec-probe-2026-10-12.json "$output/profile.json"
[[ $(sha256sum -- "$output/UkeAlohaSec.bin") == "$(jq -er '.probe_sha256' "$output/profile.json") "* ]]
cp -- scripts/make-sec-test-image.sh "$output/make-sec-test-image.sh"
cp -- docs/SEC-PROBE-BUNDLE.md "$output/README.md"
cp -- LICENSE "$output/LICENSE"
cp -- src/upstream/dtc-7a1e017926004ecff5fce62d62d42ce9f3e00082/BSD-2-Clause "$output/LICENSE.libfdt"
mkdir -- "$output/source"
cp -- src/platform/UkePlatformPkg/Sec/{ProbeEntry.S,ProbeMain.c,ProbeMemory.c,Probe.lds,FramebufferAdmission.c} "$output/source/"
cp -- manifests/sec-probe-sources-2026-10-12.json "$output/source/sources.json"
cp -- include/uke/sec_probe.h "$output/source/sec_probe.h"
cp -- src/efi/FreestandingMemory.c "$output/source/"
mkdir -- "$output/source/libfdt"
cp -- src/upstream/dtc-7a1e017926004ecff5fce62d62d42ce9f3e00082/libfdt/{fdt.c,fdt_ro.c,fdt_check.c,fdt_addresses.c,fdt.h,libfdt.h,libfdt_env.h,libfdt_internal.h} "$output/source/libfdt/"
chmod 755 -- "$output/uke-boot-tool" "$output/make-sec-test-image.sh"
cd -- "$output"
find . -type f -not -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
sha256sum --check --strict SHA256SUMS > /dev/null
! find . -type f | rg '\.(py|pyc|img|key|pem)$|(^|/)python([0-9.]*)?$'
! rg -a -l '/home/|/root/|BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY' .
zip -X -q -r ../uke-aloha-sec-probe-20261012.zip .
cd -- ..
sha256sum uke-aloha-sec-probe-20261012.zip > uke-aloha-sec-probe-20261012.zip.sha256
unzip -tq uke-aloha-sec-probe-20261012.zip
printf 'Built development ZIP; no firmware installer or OEM image included.\n'
