#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 2 ]] || { printf 'Usage: record-platform-build-evidence.sh CLANG_BIN_DIR MDEPKG_DIRECTORY\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
mde=$(realpath -- "$2")
cd -- "$component"
receipts=build/platform-receipts
mkdir -p -- "$receipts"
hash() { sha256sum -- "$1" | cut -d ' ' -f1; }
cmake --build build/release > "$receipts/release-build.log"
cmake --build build/sanitized-clang > "$receipts/sanitizer-build.log"
ctest --test-dir build/release --output-on-failure | tee "$receipts/release-tests.log"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitized-clang --output-on-failure | tee "$receipts/sanitizer-tests.log"
rg -Fq '100% tests passed, 0 tests failed out of 11' "$receipts/release-tests.log"
rg -Fq '100% tests passed, 0 tests failed out of 11' "$receipts/sanitizer-tests.log"
build/release/uke-platform-boot-tests | tee "$receipts/platform.log"
bash scripts/check-ufs-read-only.sh "$compiler" "$mde" | tee "$receipts/ufs.log"
bash scripts/check-efi-return.sh "$compiler" "$mde" | tee "$receipts/efi-return.log"
bash scripts/check-sec-entry.sh "$compiler" "$mde" | tee "$receipts/sec-entry.log"
rg -Fxq 'platform memory/return checks=104 passed' "$receipts/platform.log"
rg -Fxq 'UFS read-only EFI mock checks=89 passed; no controller/PHY execution' "$receipts/ufs.log"
rg -Fxq 'EFI Android return mock checks=136 passed; no physical CPU restoration' "$receipts/efi-return.log"
[[ $(rg -c 'QEMU-user x0-x3/SP restoration and original branch checks passed' "$receipts/sec-entry.log") == 2 ]]
sources='{}'
while IFS= read -r source; do
  sources=$(jq --arg path "$source" --arg sha "$(hash "$source")" '. + {($path): $sha}' <<< "$sources")
done < <(printf '%s\n' CMakeLists.txt include/uke/{boot_core.h,platform_boot.h,raw_entry.h} src/core/sha256.c \
  scripts/{check-sec-entry.sh,check-efi-return.sh,check-ufs-read-only.sh,record-platform-build-evidence.sh} \
  tests/{platform_boot_tests.cpp,ufs_read_only_tests.c,efi_return_tests.c,raw_resume_smoke.S}; \
  rg --files src/platform/UkePlatformPkg | sort)
headers='{}'
for header in Include/Uefi.h Include/Base.h Include/AArch64/ProcessorBind.h \
  Include/Protocol/DevicePath.h Include/Protocol/ScsiIo.h Include/Protocol/ScsiPassThruExt.h Include/Uefi/UefiSpec.h; do
  headers=$(jq --arg path "$header" --arg sha "$(hash "$mde/$header")" '. + {($path): $sha}' <<< "$headers")
done
artifacts='[]'
for artifact in build/sec-entry/UkeSecEntry.lib build/ufs-read-only/UkeUfsReadOnly.lib build/efi-return/UkeAndroidReturn.lib; do
  artifacts=$(jq --arg path "$artifact" --arg sha "$(hash "$artifact")" --argjson bytes "$(stat -c %s -- "$artifact")" \
    '. + [{path: $path, sha256: $sha, bytes: $bytes}]' <<< "$artifacts")
done
logs='{}'
for log in "$receipts"/*.log; do
  logs=$(jq --arg path "$log" --arg sha "$(hash "$log")" '. + {($path): $sha}' <<< "$logs")
done
jq -n --arg utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  --arg clang_pin "$(git -C "$compiler/.." rev-parse HEAD)" --arg mde_pin "$(git -C "$mde" rev-parse HEAD)" \
  --arg clang_sha "$(hash "$compiler/clang")" --arg ar_sha "$(hash "$compiler/llvm-ar")" \
  --argjson sources "$sources" --argjson headers "$headers" --argjson artifacts "$artifacts" --argjson logs "$logs" \
  '{schema_version: 1, record_id: "ALOHA-PLATFORM-20261006-01", generated_utc: $utc,
    profile_id: "uke-global-os3.0.303.0.wozmixm-development",
    inventory_scope: "selected platform project sources, tools and headers; not complete dependency closure",
    source_sha256: $sources, header_sha256: $headers,
    tool_inputs: {clang_repository: $clang_pin, mu_basecore: $mde_pin, clang_sha256: $clang_sha, llvm_ar_sha256: $ar_sha},
    artifacts: $artifacts, receipt_logs_sha256: $logs,
    checks: {release_ctest: "11/11", sanitizer_ctest: "11/11", platform_memory_return: "104/104",
      ufs_efi_mocks: "89/89", efi_exit_mocks: "136/136", raw_resume_qemu_user: "2/2"},
    raw_uke_firmware_built: false, full_edk2_inf_dsc_fdf_build: false, installable_zip_built: false,
    actual_efi_services_executed: false, privileged_sec_capture_executed: false,
    controller_phy_executed: false, physical_device_operation: false, copr_uploaded: false,
    archive_complete: false, python_runtime_payload: false}' > manifests/os3-platform-build-inputs-2026-10-06.json
printf 'Recorded platform library builds and scoped test receipts; firmware/ZIP/COPR gates remain open.\n'
