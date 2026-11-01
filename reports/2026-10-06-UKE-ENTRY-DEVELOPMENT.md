# 6 October 2026: Uke OS3 boot entry and platform integration

Record ID: **ALOHA-ENTRY-20261006-01**. Final combined check receipt:
2026-10-06T03:59:33Z. The owner selected Global `OS3.0.303.0.WOZMIXM`
for development. Scope: project source, host/AArch64 compilation, regular-file
OEM boot analysis, QEMU user-mode instruction/tool execution and hosted EFI mocks.

## Result

Implemented the native boot v4 codec and patch/restoration tool, the raw-entry
Android continuation baseline, executable manager/fallback lifecycle, and the
project-owned Uke platform package with its EFI dispatcher and hardware-provider
ABI. They are actual compiled source components rather than an installer shell
wrapped around the old diagnostic. They still do not provide raw Uke firmware,
hardware drivers or Android return after firmware changes CPU/device state.

| Component | Delivered behavior | Remaining boundary |
|---|---|---|
| [OS3 profile](../profiles/uke-global-os3-development.json) | Exact boot/kernel hashes, v4 layout, 96 MiB capacity and AVB observations | Offline development only; not installed or physically admitted |
| [Boot codec/tool](../src/boot/boot_image.cpp) | Bounded file parse, exact-stock-hash staging, new-file output/readback, damaged/double-patch rejection and exact backup restoration | Only direct Android continuation; real firmware insertion/relocation pending |
| [ARM64 continuation](../src/boot/AndroidContinuation.S) | Position-independent original branch/MZ replay, x0-x3/SP preserved, no MMIO/reset/pstore | Does not initialize UEFI or restore states changed by firmware |
| [Dispatcher core](../src/core/dispatch.c) | Return readiness before discovery, one manager attempt and one fallback callback | Platform callback execution is not an Android boot result |
| [Uke platform package](../src/platform/UkePlatformPkg/README.md) | Real GPT/FAT/config/image validation and firmware LoadImage/StartImage integration | Requires running EFI services and the unimplemented Uke hardware/return provider |

## Checks and artifacts

- Release CTest: **10/10**; ASan/UBSan CTest: **10/10**, retaining the explicit
  earlier vptr-runtime exclusion.
- Boot codec: **73 checks** natively and **73 under AArch64 QEMU user mode**.
- Actual OS3 file tests: **8/8 scenarios per native/AArch64 tool**, exact stock
  restoration, unchanged source hash, and identical staged image bytes.
- ARM64 entry: **2/2 instruction fixtures** preserve x0-x3/SP.
- Portable lifecycle: **7/7** fixture scenarios. Hosted EFI mocks:
  **7 lifecycle + 6 bounded-I/O scenarios**, with sanitizers/leak detection.
- Dispatcher PE: **AA64 EFI application, zero imports**, 16,896 bytes.
- Native tool: 36,816 bytes, x86_64 Linux; static ARM64 tool: 305,792 bytes,
  no interpreter/shared-library dependency; selector template: 256 bytes.

Exact source/runtime/tool/artifact and retained private receipt hashes are in
[the generated manifest](../manifests/os3-entry-build-inputs-2026-10-06.json).
Its recorder rebuilds and executes the named checks before recording success.
It is a selected input inventory, not complete source/license/archive closure.
The [development guide](../docs/KERNEL-ENTRY-DEVELOPMENT.md) documents commands,
preservation semantics, AVB behavior and verification limits.

## Material findings and corrections

1. `signature_size=0` did not mean absent embedded metadata: two AVB0 objects
   precede outer vbmeta/AVBf. The original strict body-only check failed; bounded
   parsing fixed it. Modified development bytes do not retain stale signatures.
2. Initial AArch64 linking requested unavailable libgcc/libgcc_eh. Selecting
   compiler-rt/libunwind fixed it; stripping removed debug/comment build paths.
3. Nabu's `0x9fc00000` destination falls within the OEM Uke Android 15 ADSP/SLPI
   reservation `[0x9d400000, 0xa1900000)`. That source reference rejects a direct
   port; it is not a complete OS3 runtime map.
4. Review corrected a dispatcher guard that rejected larger FAT cluster reads:
   it now splits reads into 4 KiB backend operations. Host mocks cover 8 KiB
   success and bounds/time/read-budget/read-failure rejection.
5. Hosted Mu mocks initially needed `-fshort-wchar`; a later receipt match
   interpreted a literal plus as regex syntax. Both configuration/checker
   failures were corrected and the combined run completed successfully.

These lessons are indexed as **UKE-ALOHA-IMP010–UKE-ALOHA-IMP014** in the workspace
documentation component's dated engineering record.

## Still required for Nabu-style dualboot

The [installer readiness record](../manifests/boot-installer-readiness.json)
now establishes the owner's selected OS3 input. Its other five requirements
remain incomplete. The remaining work is actual Uke SEC/PrePi/DXE and firmware
placement, SM7675 UFS controller/PHY and clock/power/SMMU integration, saved-entry
state and driver quiescence for ESP-independent Android return, real firmware
patch/relocation and an installer slot/backup/journal/write/readback transaction.

No firmware-entry mode or raw FD is enabled using guessed donor addresses.
The application refuses dispatch without its provider. This prevents a false
build claim; it does not turn a missing provider into working Android fallback.
Volume/power hardware, USB keyboard/mouse, optional touch and Linux handoff need
their separate drivers/acceptance. HDMI mirroring and Windows remain last.

**No installable dualboot ZIP exists yet.** No device was unlocked, mounted,
rebooted, flashed, repartitioned or assigned a boot slot. No own-device result
was created; the original OEM boot bytes remain unchanged.
