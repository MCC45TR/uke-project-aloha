# Uke UEFI architecture

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
