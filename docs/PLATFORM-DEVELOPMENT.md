# Uke platform libraries: 6 October 2026

Record ID: **ALOHA-PLATFORM-20261006-01**. The selected development profile is
Global **OS3.0.303.0.WOZMIXM**. These are implemented and compiled platform
libraries, not a complete raw Uke firmware target or an installable boot ZIP.

## Implemented behavior

| Layer | Source implementation | Remaining integration |
|---|---|---|
| SEC entry | Fixed 96-byte raw entry context, assembly capture of x0-x3/SP/EL/DAIF/SCTLR/timer frequency, admission of a narrow EL1/EL2 raw Linux contract | Exact-profile admission, protected context placement, actual SEC entry and exception/console backend |
| PrePi memory | Overflow-safe RAM/carveout/kernel/FDT/initrd/context checks, explicit owned aperture, aligned firmware/heap/stack layout, zero output on failure | Runtime FDT/ownership provider, HOB/MMU construction, firmware relocation and real FD/DSC/FDF |
| UFS read layer | Native UFS path hash admission, SCSI target/LUN cross-check, READ CAPACITY(16)/READ(16), 512–4096-byte geometry, unaligned bounded reads, invalidation on transport/short-read failure | SM7675 controller/PHY power, reset, clocks, SMMU/DMA, native EFI child handles and verified hardware deadline |
| Android return | Driver registration before mutation, reverse dirty-driver shutdown, stop on uncertain shutdown, real GetMemoryMap/ExitBootServices adapter, one stale-map retry, final x0-x3/SP restore and original kernel branch | Actual per-driver quiesce functions and board restoration of exception level, caches/MMU, interrupts, timers, coherency and DMA |

No provider is marked ready by these libraries. `complete_ownership` and
`RestoreReady` must come from verified platform code; setting them unconditionally
would bypass the central admission contract. An EFI application cannot safely
resume Android merely by calling the final raw assembly leaf.

The return transaction invokes the real architecture backend only after
successful EFI exit. A failed exit or a returned jump is an error and cannot
be retried as another boot attempt. After the first ExitBootServices attempt,
the adapter uses only GetMemoryMap/ExitBootServices for the single permitted
stale-key retry. It never allocates, logs, frees EFI memory, or accesses devices
in that interval. Retained map memory is intentionally not freed after an exit
attempt because services may have partially shut down.

The raw resume leaf branches to the preserved **second** kernel instruction,
replaying the recognized MZ instruction when required. It avoids re-entering
the patched first instruction. Privileged capture is compiled only; QEMU user
mode checks the final ABI leaf and cannot validate privileged restoration.

## Uke source findings

OEM device-tree pin `94c84aeb9e517dbde546fc732033c184094c8e57`,
`qcom/uke-sm8635-overlay.dts`, has root compatibles `qcom,cliffs-mtp`,
`qcom,cliffs`, `qcom,mtp` and `xiaomi,miboard-id = <0xB 0>`. A validator that
requires only `xiaomi,uke` would reject this OEM source. Conversely, generic
Cliffs compatibility alone does not bind a board or OS3 firmware profile.

At the same pin, `qcom/cliffs.dtsi` separates the PHY resource
`0x1d80000/0x2000` from the host `0x1d84000/0x3000` and declares additional
ICE/HWKM/MCQ resources and clock/reset/power dependencies. These are research
inputs from Android 15 source, not permission to touch MMIO or proof that OS3
has the same complete runtime contract.

The retained Mu UFS driver at `762a65dad79e2934cddbefa467b7e2c7b5ee4bc0`
encodes common LU n in byte one of its SCSI LUN and uses target/PUN zero.
The adapter verifies that full encoding before a data command. The caller
must obtain the path and SCSI protocol from the **same** EFI child handle.
The generic driver's `ResetChannel`/`ResetTargetLun` return success without
performing a hardware reset. Those callbacks cannot establish DMA shutdown
and must not become the Android-return quiescence backend.

The raw Linux handoff requirements include DMA shutdown and cache/MMU/interrupt
state preparation. [Linux boot ABI](https://docs.kernel.org/arch/arm64/booting.html).
The boot-services exit adapter follows the stale-memory-map exit contract.
[UEFI Boot Services](https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html#efi-boot-services-exitbootservices).

## Build and verification

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
bash scripts/check-sec-entry.sh "$CLANG_BIN_DIR" "$MDEPKG_DIR"
bash scripts/check-ufs-read-only.sh "$CLANG_BIN_DIR" "$MDEPKG_DIR"
bash scripts/check-efi-return.sh "$CLANG_BIN_DIR" "$MDEPKG_DIR"
bash scripts/record-platform-build-evidence.sh "$CLANG_BIN_DIR" "$MDEPKG_DIR"
```

The recorder also requires the existing configured `build/sanitized-clang`
tree described in the build guide. It rebuilds and executes checks before
recording success. Shell/C/C++/assembly tools are host dependencies; no project
Python or tablet Python runtime is used.

| Receipt | Result | Evidence boundary |
|---|---|---|
| Release / ASan / UBSan / leak checks | 11/11 suites in each build | Native host fixtures |
| SEC / PrePi / driver return | 104 checks | Synthetic ranges and driver callbacks |
| UFS read adapter | 89 checks | EFI SCSI mocks; no controller or PHY execution |
| EFI exit adapter | 136 checks | Mock services; no actual firmware exit |
| Raw final resume | 2/2 ARM64 instruction fixtures | QEMU user mode; no privileged state restoration |
| SEC capture, memory and return libraries | ARM64 COFF archives built | Standalone compiler; INF/DSC/FDF not built through EDK II |

The first UFS host compile failed because DevicePath.h preceded Uefi.h.
Including the adapter header first corrected the EFI type dependency. Review
then added full SCSI target/LUN cross-checks; the initial 82-check receipt is
superseded by the final 89-check receipt.

The [input and artifact manifest](../manifests/os3-platform-build-inputs-2026-10-06.json)
records selected source/header/tool hashes, actual archive hashes and private
log hashes. Its inventory is deliberately not full dependency closure.

## Delivery state

There is no complete Uke SEC/PrePi/DXE firmware, physical UFS initialization,
operational post-firmware Android backend or installable dualboot ZIP. These
libraries provide concrete pieces of that implementation. Their tests do not
close the [installer readiness](../manifests/boot-installer-readiness.json) gate.

Read-only COPR inventory found twelve registered sources in
`mcc45tr/uke-linux-test`, with no Aloha boot-firmware source. No package was
uploaded or registered because the requested readiness condition was not met.
The separate `xiaomi-uke-firmware` Linux blob RPM also remains blocked by its
file-level provenance/license/hash/kernel-request admission; it is not an
Aloha firmware-volume package.

No unlock, flash, partition access, reboot, slot change, manager boot, hardware
test or COPR mutation was performed. Preserve the stock early firmware and
recovery route. Windows remains last; preboot HDMI mirroring remains the final
firmware feature.
