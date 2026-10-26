# Uke UEFI architecture

The dated [implementation plan](UKE-PORT-PLAN.md) expands this contract into
source, firmware, boot-manager, physical-input and display acceptance milestones.
Its first target is Fedora/Linux with Android return; rEFInd and systemd-boot
receive separate compatibility records, and external preboot mirroring is last.

Normal startup discovers an eligible partition named `ESP` using validated Uke media/GPT identity
and loads its selected AArch64 manager. Missing, invalid or ambiguous ESPs and
missing or invalid managers lead to ordinary Android. This fallback must work
without the ESP and without resetting into the same dispatcher indefinitely.
Discovery is read-only; recovery owns any partition creation or repair.

The [12 October selection policy](BOOT-POLICY.md) opens the installed
`EFI/BOOT/BOOTAA64.EFI` default or discovers a unique ARM64 EFI application
below `EFI` without mandatory project configuration. The shared probe and EFI
caller are implemented; their hardware provider and raw firmware entry remain
separate prerequisites. The SEC marker ZIP does not execute this dispatcher.

The initial [portable core and host backend](BOOT-POLICY.md) implement this
decision contract on disposable GPT/FAT32 images. The [implementation record](IMPLEMENTATION-STATUS.md)
separates those checks from the missing Uke UFS, manager dispatch and
ESP-independent Android-return backends. The standalone EFI diagnostic only
inventories existing Block I/O; returning from it is not Android-return evidence.

The planned multiboot allocation separates Android userdata, a shared ESP,
Linux and a future Windows area within measured userdata capacity. Exact geometry,
encrypted-data migration and Android/OTA compatibility belong to recovery's
write/restore gate. Windows is the last expansion, after the firmware input and
display milestones; an ESP does not establish Windows driver or boot support.

The inspected Aloha tree does not provide a Uke/SM7675 target. This component will implement a real platform, not relabel another device's binary. The first tasks are full dependency pinning and a stock-derived platform contract after recovery foundations are ready.

The platform contract covers memory map and reserved regions, firmware-owned resources, interrupt/cache/MMU state, clocks, diagnostics, framebuffer/GOP, input, UFS Block I/O, EFI-stub/DT handoff and ExitBootServices. Bring up storage read-only before any write path. Preserve stock XBL, ABL and trusted firmware.

Dual boot and Fedora single-user-OS profiles both retain a recovery route and a verified return to stock. A/B slots do not isolate userdata. UEFI image identity, selected firmware profile and recovery state must be visible to the recovery manager. UKI/boot-manager integration follows proven EFI handoff.

EDK II platform code follows upstream C and assembly requirements. New native companion tools use C++; no Python is deployed to the tablet. Pin and document any unavoidable upstream host build tool separately.

## Recovery one-shot boot contract

The recovery [comprehensive roadmap](https://github.com/MCC45TR/orangefox_device_xiaomi_uke/blob/codex/ure-rescue-framework/docs/COMPREHENSIVE-ROADMAP.md#27-project-aloha-integration)
and workspace [URE-09 milestone](https://github.com/MCC45TR/uke-linux/blob/codex/ure-roadmap-integration/PLAN.md#61-ure-implementation-milestones)
add a planned direct Android/Linux/Windows boot interface. “Direct” means the
user need not interact with a boot menu; it still requires a proven low-level
routing backend. This contract is not implemented and establishes no boot or
Windows compatibility result.

The shared request records schema version, request ID, exact target and entry
IDs, selected firmware/profile, one-shot/default intent and expected boot
components. Recovery validates root/encryption/subvolume, kernel/initramfs/DT/
ESP or Windows loader/BCD as appropriate. Aloha rejects unknown versions,
malformed targets, stale/replayed requests and paths outside approved entries.
Consume a one-shot request durably before handoff, retain the configured default
and preserve timeout, invalid-request, Android-return and recovery fallbacks.
An ESP request file such as `/EFI/UKE/next-boot.json` is a proposed backend;
EFI BootNext/loader variables require separately proven availability and
persistence. Shared ESP files belonging to other OSes remain protected.

Boot history distinguishes requested, consumed, acknowledged, failed and unknown
attempts. Recovery re-entry alone does not prove a failed boot; correlate
acknowledgment or crash evidence. Test parser/version/replay/consumption failures
on host fixtures and actual routing/return at H2 only after physical platform
handoff is accepted. Source implementation and emulation remain separate from
device acceptance.
