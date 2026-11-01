#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 4 ]] || { printf 'Usage: record-entry-build-evidence.sh CLANG_BIN_DIR MUSL_SYSROOT MDEPKG_DIR OS3_STOCK_BOOT\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
sysroot=$(realpath -- "$2")
mde=$(realpath -- "$3")
stock=$(realpath -- "$4")
hash() { sha256sum -- "$1" | cut -d ' ' -f1; }
[[ $(hash "$stock") == efdee1d4e1acd7f6e77615330dbcb045caeeccad8568d89fa6abd627f606e77f ]]
cd -- "$component"
receipts="$component/build/entry-receipts"
mkdir -p -- "$receipts"
# Rebuild and execute the checks; a recorder must not stamp hard-coded success
# onto stale artifacts after a source change. Preserve existing build caches.
cmake --build build/release > "$receipts/release-build.log"
cmake --build build/sanitized-clang > "$receipts/sanitizer-build.log"
ctest --test-dir build/release --output-on-failure | tee "$receipts/release-tests.log"
ctest --test-dir build/sanitized-clang --output-on-failure | tee "$receipts/sanitizer-tests.log"
rg -q '100% tests passed, 0 tests failed out of 11' "$receipts/release-tests.log"
rg -q '100% tests passed, 0 tests failed out of 11' "$receipts/sanitizer-tests.log"
build/release/uke-boot-image-tests | tee "$receipts/boot-codec-native.log"
build/release/uke-dispatch-tests | tee "$receipts/dispatch-native.log"
bash scripts/build-arm64-boot-tool.sh "$compiler" "$sysroot" > "$receipts/arm64-build.log"
qemu-aarch64 build/arm64-boot-tool/uke-boot-image-tests | tee "$receipts/boot-codec-arm64.log"
rg -q -x 'boot-image checks=73 passed' "$receipts/boot-codec-native.log"
rg -q -x 'boot-image checks=73 passed' "$receipts/boot-codec-arm64.log"
rg -q '^dispatch: 7/7' "$receipts/dispatch-native.log"
bash scripts/check-android-continuation.sh "$compiler" | tee "$receipts/entry-arm64.log"
[[ $(rg -c 'QEMU-user x0-x3/SP checks passed' "$receipts/entry-arm64.log") == 2 ]]
bash tests/os3-boot-roundtrip.sh build/release/uke-boot-tool build/android-continuation/AndroidContinuation.bin "$stock" | tee "$receipts/os3-native.log"
bash tests/os3-boot-roundtrip.sh build/arm64-boot-tool/uke-boot-tool build/android-continuation/AndroidContinuation.bin "$stock" qemu-aarch64 | tee "$receipts/os3-arm64.log"
rg -q '8/8 passed' "$receipts/os3-native.log"
rg -q '8/8 passed' "$receipts/os3-arm64.log"
bash scripts/build-efi-dispatcher.sh "$compiler" "$mde" > "$receipts/dispatcher-build.log"
bash scripts/check-efi-dispatcher-host.sh "$compiler" "$mde" | tee "$receipts/dispatcher-host.log"
rg -Fq '7 lifecycle + 6 bounded-I/O scenarios passed' "$receipts/dispatcher-host.log"
sources='{}'
while IFS= read -r source; do
  sources=$(jq --arg path "$source" --arg sha "$(hash "$source")" '. + {($path): $sha}' <<< "$sources")
done < <(printf '%s\n' CMakeLists.txt include/uke/boot_core.h include/uke/boot_image.hpp \
  src/boot/AndroidContinuation.S src/boot/boot_image.cpp src/host/boot_tool.cpp \
  src/core/{gpt,policy,dispatch,sha256,fat32}.c \
  src/platform/UkePlatformPkg/{UkePlatformPkg.dec,Include/Protocol/UkeBootPlatform.h,BootDispatcher/BootDispatcher.c,BootDispatcher/BootDispatcher.inf} \
  tests/{boot_image_tests.cpp,dispatch_tests.cpp,efi_dispatch_tests.c,android_continuation_smoke.S,boot-tool-files.sh,os3-boot-roundtrip.sh} \
  scripts/{build-android-continuation.sh,check-android-continuation.sh,build-arm64-boot-tool.sh,build-efi-dispatcher.sh,check-efi-dispatcher-host.sh,record-entry-build-evidence.sh})
runtime='{}'
for library in libc++.a libc++abi.a libunwind.a; do
  runtime=$(jq --arg name "$library" --arg sha "$(hash "$compiler/../lib/aarch64-unknown-linux-musl/$library")" '. + {($name): $sha}' <<< "$runtime")
done
runtime=$(jq --arg libc "$(hash "$sysroot/lib/libc.a")" \
  --arg crt "$(hash "$compiler/../lib/clang/20/lib/aarch64-unknown-linux-musl/libclang_rt.builtins.a")" \
  '. + {"libc.a": $libc, "compiler-rt-builtins": $crt}' <<< "$runtime")
artifacts='[]'
for artifact in build/release/uke-boot-tool build/arm64-boot-tool/uke-boot-tool \
    build/android-continuation/AndroidContinuation.bin build/efi-dispatcher/UkeBootDispatcher.efi; do
  artifacts=$(jq --arg path "$artifact" --arg sha "$(hash "$artifact")" \
    --argjson bytes "$(stat -c %s -- "$artifact")" \
    '. + [{path: $path, sha256: $sha, bytes: $bytes}]' <<< "$artifacts")
done
test_logs='{}'
for log in "$receipts"/*.log; do
  test_logs=$(jq --arg path "build/entry-receipts/$(basename -- "$log")" --arg sha "$(hash "$log")" '. + {($path): $sha}' <<< "$test_logs")
done
jq -n --arg utc "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
  --arg clang_pin "$(git -C "$compiler/.." rev-parse HEAD)" \
  --arg sysroot_pin "$(git -C "$sysroot" rev-parse HEAD)" \
  --arg mde_pin "$(git -C "$mde" rev-parse HEAD)" \
  --arg clang_sha "$(hash "$compiler/clang")" --arg lld_sha "$(hash "$compiler/ld.lld")" \
  --arg host_cxx "$(/usr/bin/c++ --version | head -n1)" --arg host_cxx_sha "$(hash /usr/bin/c++)" \
  --argjson sources "$sources" --argjson runtime "$runtime" --argjson artifacts "$artifacts" --argjson test_logs "$test_logs" \
  '{schema_version: 1, record_id: "ALOHA-ENTRY-20261006-01", generated_utc: $utc,
    profile_id: "uke-global-os3.0.303.0.wozmixm-development",
    input_inventory_scope: "selected project sources and runtime archives; not full dependency closure",
    sources_sha256: $sources,
    tool_inputs: {clang_repository: $clang_pin, build_tools_repository: $sysroot_pin,
      mu_basecore: $mde_pin, clang_sha256: $clang_sha, lld_sha256: $lld_sha,
      host_cxx_version: $host_cxx, host_cxx_sha256: $host_cxx_sha,
      arm64_runtime_sha256: $runtime}, artifacts: $artifacts,
    checks: {release_ctest: "11/11", sanitizer_ctest: "11/11",
      boot_codec_native: "73/73", boot_codec_qemu_user: "73/73",
      os3_native_scenarios: "8/8", os3_qemu_user_scenarios: "8/8",
      entry_qemu_user_fixtures: "2/2", portable_dispatch_lifecycle: "7/7",
      efi_host_lifecycle: "7/7", efi_host_io: "6/6"}, receipt_logs_sha256: $test_logs,
    physical_device_operation: false, actual_efi_services_executed: false,
    raw_uke_firmware_built: false, installable_zip_built: false,
    archive_complete: false, stock_image_redistributed: false,
    python_runtime_payload: false}' > manifests/os3-entry-build-inputs-2026-10-06.json
printf 'Recorded selected entry/codec/dispatcher build inputs and scoped receipts.\n'
