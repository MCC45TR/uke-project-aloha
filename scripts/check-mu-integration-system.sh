#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
set -euo pipefail
[[ $# == 1 ]] || { printf 'Usage: check-mu-integration-system.sh QEMU_CONTAINER_IMAGE\n' >&2; exit 2; }
component=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
output="$component/build/mu-integration/system"
efi="$component/build/mu-integration/Build/RELEASE_CLANGPDB/AARCH64"
mkdir -p -- "$output/esp/EFI/BOOT"
cp -- "$efi/MuDispatchSmoke.efi" "$output/esp/EFI/BOOT/BOOTAA64.EFI"
cp -- "$efi/UkeBootDispatcher.efi" "$output/esp/UkeBootDispatcher.efi"
for mode in present absent; do
  if [[ $mode == absent ]]; then rm -- "$output/esp/UkeBootDispatcher.efi"; fi
  timeout --foreground 45s podman run --rm --network=none \
    -v "$output:/test:ro,Z" --entrypoint qemu-system-aarch64 "$1" \
    -machine virt,gic-version=3 -cpu cortex-a76 -m 512M \
    -bios /usr/share/edk2/aarch64/QEMU_EFI.fd -display none -monitor none -serial stdio \
    -drive file=fat:ro:/test/esp,format=raw,if=none,id=esp,readonly=on \
    -device virtio-blk-pci,drive=esp > "$output/$mode.log" 2>&1
  if [[ $mode == present ]]; then
    rg -q 'UKE-MU-QEMU: missing platform safely refused; actual EFI StartImage passed' "$output/$mode.log"
    ! rg -q 'UKE-MU-QEMU: FAIL' "$output/$mode.log"
  else
    rg -q 'UKE-MU-QEMU: FAIL' "$output/$mode.log"
    ! rg -q 'UKE-MU-QEMU: missing platform safely refused' "$output/$mode.log"
  fi
  printf 'QEMU-system actual Mu EFI application: %s passed\n' "$mode"
done
