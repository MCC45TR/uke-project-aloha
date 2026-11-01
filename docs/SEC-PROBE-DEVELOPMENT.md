# Uke SEC entry development — 12 October 2026

Record: **ALOHA-SEC-20261012-01**. This advances the Uke Aloha port to a local
RAM-boot candidate based on the owner's working Fedora pair. It is a privileged
entry/display/continuation test; a full Mu SEC/PrePi/DXE firmware remains open.
Read the [bundle and physical test guide](SEC-PROBE-BUNDLE.md) before a trial.

## Source refresh and the working Linux baseline

The live [Aloha main tree](https://github.com/Project-Aloha/mu_aloha_platforms/tree/7c81a0c77fbf77bc28a305e1e9d075f53648f93b)
still has no Uke/SM7675 target. Its Mu BaseCore gitlink is
`562c0927ef7bfc65d26d9caf322e569ccb23b6a9`, now imported from a hash-checked
archive for the existing ARM64 EFI component builds. General SEC/PrePi/DXE
infrastructure exists upstream; the missing work is its Uke platform binding.
The [direct Uke Linux source](https://github.com/ztsubaki/uke-linux/tree/32cad9ccd383ff4b37d8a5e7f88a8bfdcc07307e)
and [latest Fedora donor change](https://github.com/linuxgamer/uke-fedora/commit/cf4f9317a7fef3e9bdddc36bedcb257fa51b0994)
were checked separately. The latest Fedora commit changes its README, not an EFI
storage or PHY implementation. See the [source lock](../manifests/sec-probe-sources-2026-10-12.json).

The owner's working Linux record is [pair-0242a33799d12541](https://github.com/MCC45TR/uke-linux-docs/blob/main/docs/testing/FEDORA-HARDWARE-HANDOFF-2026-10-11.md):
stock-ABL RAM boot, local screen TTY, UFS-backed EXT4 and USB ECM SSH. The exact
original 96 MiB boot file hash is
`b32bad9905a7a0db686e1c45623da1c093cdaaafa81c8e8544a63cb17b7d5f98`.
The private original FDT passes the new read-only framebuffer admission check.
Neither the FDT nor the OEM boot file is redistributed in the development ZIP.

The local kernel's `src/boot/uke-abl.c` supplies the validated handoff method:
3200 × 2136, 12,800-byte stride, `a8r8g8b8`, with the address derived at runtime
from `/reserved-memory/splash_region`. The probe rejects a foreign model,
incompatible cells, malformed FDT, invalid/short/overflowed RAM range, reusable
or overlapping reservations, and kernel/FDT overlap. The retained OEM model
string says `Uke based on SM8635`; this is an exact FDT admission string, not a
claim that Uke's physical SoC is SM8635. No Nabu memory address is copied.

## Implemented path

`ProbeEntry.S` admits raw EL1/EL2 with MMU/data cache off and IRQ/FIQ masked.
It checks normal x0–x3 boot arguments, FDT header bounds and FDT/probe overlap
before using the private 32 KiB stack inside the expanded Image. C uses pinned
read-only libfdt and validates the framebuffer; a small visual marker is the
only peripheral write. The assembly restores x0–x3/SP, replays the original
MZ instruction when needed, and branches to the original kernel entry.

The C++ boot codec appends after the greater of kernel file length and original
Image memory size, preserving the original BSS extent. It retains container
capacity, removes stale AVB metadata and records both input hashes. Exclusive
output creation, durable readback, exact backup reconstruction, duplicate-patch
rejection and the existing regular-file boundary apply to both probe and older
Android-only continuation commands. Unsigned development output is intentional;
this is not a signing or installed-slot compatibility service.

The first QEMU-system trial faulted on an unaligned 64-bit FDT load while the
MMU was off. `-mstrict-align` prevents compiler-merged unaligned access;
`-mgeneral-regs-only` avoids an uninitialized FP/SIMD dependency. The final raw
payload has no undefined symbols, relocations, interpreter or runtime imports.
The raw probe build uses no Python. The separate Mu component DSC now uses the
documented, pinned [host-only BaseTools dependency](MU-BUILD-DEPENDENCY.md).
No Python tool or runtime is packaged.

## Reproduce

Use the retained host Clang `clang-r547379` and matching AArch64 musl sysroot;
their locations are supplied by the operator and are not embedded in public
records. All imported sources remain under `src/upstream/`; archives remain
under this component's `referances/snapshots/`.

```sh
scripts/prepare-sec-probe-sources.sh
cmake -S . -B build/entry-20261012 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/entry-20261012 -j4
ctest --test-dir build/entry-20261012 --output-on-failure
scripts/build-sec-probe.sh "$aloha_clang_dir" "$aloha_musl_sysroot"
scripts/check-sec-probe-system.sh "$aloha_clang_dir" build/entry-20261012 "$aloha_qemu_image"
tests/sec-probe-files.sh build/entry-20261012/uke-boot-tool \
  build/sec-probe/UkeAlohaSec.bin build/sec-probe/system/branch.boot
scripts/build-arm64-boot-tool.sh "$aloha_clang_dir" "$aloha_musl_sysroot"
qemu-aarch64 build/arm64-boot-tool/uke-boot-image-tests
scripts/build-mu-integration.sh "$aloha_clang_dir"
scripts/check-mu-integration-system.sh "$aloha_qemu_image"
scripts/package-sec-probe.sh
```

The QEMU container used for the receipt is the existing
`localhost/uke-qemu-console-host:20261006`; it provides `qemu-system-aarch64`.
It emulates `virt`, not Qualcomm hardware. The script tests branch/MZ entries
at EL1/EL2 and a foreign-FDT bypass. Each case must draw or skip as specified,
resume with the original x0–x3/SP, and exit successfully through semihosting.
QEMU addresses occur only in the synthetic fixture, never in the Uke profile.

Sanitizer commands follow the existing [build guide](BUILD.md), using a new
`build/entry-sanitized-20261012` directory. Current results are 12/12 CTest
suites in each host configuration, 93 codec checks on native/ARM64, 37 synthetic
framebuffer checks, the separately admitted retained FDT, five system-emulation
cases and eight CLI rejection scenarios. Local Fedora and OS3 output/restore
hashes are recorded in the [build receipt](../manifests/sec-probe-build-2026-10-12.json).
The fixture-writing commands add their own file assertions (38 and 39 checks);
these are not extra parser cases. System fixtures also verify the actual EL.

## Actual Mu integration and EFI execution

`UkeBringup.dsc` builds the dispatcher, SEC/PrePi memory library, read-only
UFS/SCSI adapter and Android-return library through the pinned Mu metadata build.
The separate `MuDispatchSmoke` test application starts the resulting dispatcher
through actual QEMU firmware `LoadImage`/`StartImage` services. With no Uke
platform provider, the dispatcher must return `EFI_NOT_READY`. A missing-image
negative control must report failure. Both cases passed. This establishes real
generic EFI execution, not Uke UFS access, ESP selection or Android return.

The first metadata build exposed absent native UUID headers/link inputs and the
MdePkg DEC's required MIPI submodule include. They were resolved with checked
host-only RPM extraction and the exact Mu gitlink. GCC 16's unused-but-set
warning needed a narrow BaseTools-host suppression. Final EFI linking exposed
missing compiler `memcpy`/`memset`; the DSC now binds Mu's own
`CompilerIntrinsicsLib`. Source imports remain immutable. Full firmware FDF/FD,
complete recursive source closure and a Uke platform provider remain open.

## Remaining full-firmware work

The expanded Image supplies a concrete small probe aperture; it does not yet
supply the admitted heap, HOBs, page tables, CPU exceptions, controller ownership
and state restoration for a full DXE session. Uke UFS controller/Palawan PHY,
power/clocks/resets/SMMU/DMA, GOP/input/USB protocols and ESP-independent Android
return after those changes still need integration. The existing SCSI adapter
requires a preexisting EFI UFS handle and does not initialize this hardware.

The next physical gate is the marker followed by the known Fedora console/SSH
on an unchanged partition layout. After that, the captured entry state and
verified aperture can guide the full Mu target. The complete boot-installer
readiness contract remains unavailable; a development ZIP must not turn that
gate green or trigger a COPR firmware upload.
