# Initial implementation record: 5 October 2026

The [12 October ESP update](ESP-MANAGER-DEVELOPMENT.md) supersedes mandatory
project configuration/fixed manager paths with automatic installed-manager
discovery on a validated partition named `ESP`. Later EFI and Mu builds remain
component evidence; full Uke firmware is still incomplete.

The [12 October SEC entry record](SEC-PROBE-DEVELOPMENT.md) adds the first
raw boot-image test candidate, a real Mu component DSC build and generic EFI
execution in QEMU firmware. It does not close the Uke UEFI/installer limitations
in this historical record; earlier test counts below remain dated evidence.

Record ID: **ALOHA-CORE-20261005-01**. Evidence: project source, native host
fixtures, standalone AArch64 compilation and QEMU user-mode execution.
Final test review: 2026-10-05T19:46:18Z. No tablet operation occurred.

The first implementation slice is complete: read-only GPT/ESP discovery,
restricted FAT32/config/manager preflight, default-Android decision policy and
volume/power event handling. These are reusable parts of the planned platform.
There is no bootable Uke firmware or operational Android-return backend yet.

The requested end product is the [Android boot-patching installer ZIP](BOOT-INSTALLER-CONTRACT.md).
The diagnostic build is an intermediate component and does not satisfy that
delivery. The packaging review records the missing platform prerequisites.

The [6 October rebuild](../reports/2026-10-06-CORE-BUILD.md) adds a successful
optimized Release host build, seven fresh host suites and nine file scenarios,
plus an identical EFI diagnostic and a repeated ARM64 QEMU-user smoke run.

The subsequent [OS3 entry development](KERNEL-ENTRY-DEVELOPMENT.md), record
ALOHA-ENTRY-20261006-01, adds the native boot v4 codec, AArch64 tool, direct
Android continuation and Uke platform protocol/EFI dispatcher. The current
Release and sanitizer suites are 10/10, with 73 boot-codec checks, eight actual
OS3 scenarios on each native/QEMU-user tool, two entry instruction fixtures,
and hosted EFI callback/I/O checks. These additions do not provide raw firmware
or Android return after hardware initialization. Older receipts below remain
historical rather than being relabeled as current results.

The [platform library development](PLATFORM-DEVELOPMENT.md), record
ALOHA-PLATFORM-20261006-01, adds SEC entry capture/admission, PrePi memory
checks, a UFS/SCSI read-only adapter, driver shutdown ordering, real EFI exit
and the final raw Android resume leaf. Current Release/sanitizer suites are
11/11; additional checks are 104 platform, 89 UFS, 136 EFI-exit and two
QEMU-user resume fixtures. Complete raw firmware, Uke controller/PHY and
physical CPU/device restoration remain open.

## Implemented source

| Component | Actual behavior |
|---|---|
| [C core API](../include/uke/boot_core.h) and [modules](../src/core) | Validates both GPT copies, FAT32 owned paths, exact config profile, AArch64 PE and SHA-256; chooses a unique eligible ESP or requests Android |
| [Button state machine](../src/core/buttons.c) | Debounce, bounded volume repeat, short power release, held-at-start/chord suppression and backwards-clock handling |
| [C++ host inspector](../src/host/esp_inspect.cpp) | Real regular-file image preflight; no mounts, storage writes or OS dispatch |
| [EFI diagnostic](../src/efi/EspProbe.c) | Read-only GPT inventory through preexisting EFI Block I/O; returns to its caller |
| [Source identity audit](../scripts/audit-source-closure.sh) | Finds exact and missing recursive checkout nodes without fetching or promoting archive completeness |
| [Preparation profile](../profiles/poco-pad-x1-global-os2.json) | Dated stock identity; unknown entry, return, memory and physical geometry fields remain null |

Read the [boot policy](BOOT-POLICY.md), [platform contract](PLATFORM-CONTRACT.md)
and [build commands](BUILD.md) before extending these parts.

## Verification receipts

| Receipt | Result | Limit |
|---|---|---|
| HOST-01: Debug native CMake/CTest | 7/7 passed: GPT, policy, config, image, buttons, FAT32, inspector boundary | Disposable synthetic fixtures; no actual UFS or keys |
| SAN-01: Clang ASan/UBSan with leak detection | 7/7 passed; C++ vptr instrumentation explicitly disabled | See documented GCC/runtime failures and correction below |
| FAT-01: Native real FAT32/GPT file round trips | 9/9 passed, input hashes unchanged | EFI diagnostic copied under fixture manager names; no actual manager executed |
| FAT-SAN-01: Same round trips with sanitized inspector/builder | 9/9 passed | Real host file parsing, not firmware filesystem execution |
| COFF-01: Standalone application | ARM64 `AA64`, EFI application subsystem, zero imports; all five core modules compile | Application links probe/GPT only; no Uke platform DSC/FDF build |
| REPRO-01: Consecutive application rebuilds | Identical SHA-256 after setting PE timestamp zero | Same-host narrow artifact repeatability, not offline/full-platform reproducibility |
| ARM64-01: QEMU user-mode smoke ELF | SHA/CRC/config/policy/buttons passed | Linux ABI test program; no EFI services, firmware or hardware execution |
| SOURCE-01: Recursive checkout identity audit | 32 exact nodes including root; 12 missing nodes | No complete archive, license review or offline restore |

The GPT suite checks 512- and 4096-byte geometry, independent fixture CRCs,
overlap, GUID/bounds failures and 1000 deterministic image mutations. The SHA
suite uses empty, `abc`, padding-boundary and million-`a` reference vectors.
Button fixtures include 100 power press/release cycles. The disk round trips
cover valid rEFInd-path and systemd-boot-path payloads, no ESP, two eligible ESPs,
wrong profile, missing manager, wrong hash, damaged FAT and damaged GPT.

Application SHA-256:
`cf634faf9a863dd29c078e0c116b526438a1b907c70e784af504de109d3a827f`.
The [build input manifest](../manifests/initial-core-build-inputs-2026-10-05.json)
retains actual header/source/tool hashes. Artifact binaries stay under ignored
`build/`; no firmware download or flash command is published.

## Failed trials and corrections

- Initial sanitizer compilation exposed promoted integer conversion warnings;
  explicit whole-expression casts corrected the byte-decoding and key masks.
- The host GCC sanitizer link failed because its ASan shared library was absent.
  Reusing pinned host Clang avoided a global host-package repair.
- Clang C++ UBSan vptr linkage lacked runtime symbols. An explicit build option
  disabled only vptr instrumentation; the remaining sanitizer suites passed.
- PE linker timestamps initially changed the artifact hash. `/timestamp:0`
  made consecutive narrow diagnostic rebuilds byte-identical.
- Review found GPT candidates could remain partially counted on a later scan
  error. Count publication now waits for complete validation, with negative and
  mutation assertions covering the correction.
- A later ineligible callback result could overwrite the success reason of an
  already selected manager. The policy now preserves the selected result;
  a dedicated negative callback fixture covers the correction.
- Recursive Git inspection initially inherited the parent repository for empty
  submodule directories. Checking the exact repository top level classifies
  these as missing checkouts. Donor files were not modified.
- New shell entrypoints initially lacked executable mode. Their modes were
  corrected before final direct-command verification.

## Next platform work and unchanged gates

M1 has an identity audit and narrow header/tool lock; recursive archive/license
closure remains open. M2 has an explicit preparation profile; entry, RAM ownership
and ESP-independent return remain unknown. M3 has a standalone EFI diagnostic,
without Uke firmware. M4 and M6 have portable input/discovery fixtures, without
physical drivers. These partial receipts do not close any hardware milestone.

The next required implementation input is the exact-profile entry/return and
memory contract. After it is established, create the Uke SEC/PrePi/DXE target,
minimal timer/console, EFI input adapter and read-only UFS backend. Then qualify
actual rEFInd and systemd-boot AArch64 artifacts and Linux EFI/DT handoff. USB
keyboard/mouse follow, optional native touch follows those, preboot HDMI mirroring
is the final firmware feature, and Windows remains the last expansion.

No Linux/Android boot, physical button/USB/touch/GOP/mirror result, storage
migration, unlock, reboot or flash was performed. Recovery owns any future
userdata-to-Android/ESP/Linux/Windows migration and its restoration gate.
