#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 3 ]] || { printf 'Usage: check-sec-probe-system.sh CLANG_BIN_DIR HOST_BUILD QEMU_CONTAINER_IMAGE\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
compiler=$(realpath -- "$1")
host=$(realpath -- "$2")
container=$3
output="$component/build/sec-probe/system"
mkdir -p -- "$output"
"$host/uke-sec-framebuffer-tests" --write-fixture "$output/fixture.dtb"
"$host/uke-sec-framebuffer-tests" --write-foreign-fixture "$output/foreign.dtb"
probe="$component/build/sec-probe/UkeAlohaSec.bin"
probe_hash=$(sha256sum -- "$probe"); probe_hash=${probe_hash%% *}
for mode in branch mz foreign; do
  flags=()
  dtb=fixture
  virtualizations=(off on)
  expected='framebuffer and original x0-x3/SP resume passed'
  [[ $mode != mz ]] || flags=(-DMZ_ENTRY=1)
  if [[ $mode == foreign ]]; then
    flags=(-DEXPECT_BYPASS=1)
    dtb=foreign
    virtualizations=(off)
    expected='rejected FDT and original x0-x3/SP resume passed'
  fi
  "$compiler/clang" --target=aarch64-none-elf "${flags[@]}" -c "$component/tests/sec_probe_system.S" -o "$output/$mode.o"
  "$compiler/ld.lld" -Ttext=0 --oformat=binary -e _start "$output/$mode.o" -o "$output/$mode.Image"
  "$host/uke-boot-image-tests" --write-probe-fixture "$output/$mode.Image" "$output/$mode.boot"
  boot_hash=$(sha256sum -- "$output/$mode.boot"); boot_hash=${boot_hash%% *}
  rm -f -- "$output/$mode.staged.boot"
  "$host/uke-boot-tool" stage-sec-probe "$output/$mode.boot" "$probe" "$boot_hash" "$probe_hash" \
    "$output/$mode.staged.boot" --unsigned-development
  dd if="$output/$mode.staged.boot" of="$output/$mode.staged.Image" bs=4096 skip=1 status=none
  for virtualization in "${virtualizations[@]}"; do
    timeout --foreground 15s podman run --rm --network=none \
      -v "$output:/test:ro,Z" --entrypoint qemu-system-aarch64 "$container" \
      -machine "virt,virtualization=$virtualization,gic-version=3" -cpu cortex-a76 -m 512M \
      -kernel "/test/$mode.staged.Image" -dtb "/test/$dtb.dtb" -display none -monitor none -serial none \
      -semihosting-config enable=on,target=native > "$output/$mode-$virtualization.log" 2>&1
    rg -q "^UKE-SEC-QEMU: $expected$" "$output/$mode-$virtualization.log"
    level=1
    [[ $virtualization != on ]] || level=2
    rg -qx "UKE-SEC-QEMU: actual EL$level" "$output/$mode-$virtualization.log"
    printf 'QEMU-system SEC probe: %s / virtualization=%s passed\n' "$mode" "$virtualization"
  done
done
