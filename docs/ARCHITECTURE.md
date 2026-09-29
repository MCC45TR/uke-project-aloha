# Uke UEFI architecture

The inspected Aloha tree does not provide a Uke/SM7675 target. This component will implement a real platform, not relabel another device's binary. The first tasks are full dependency pinning and a stock-derived platform contract after recovery foundations are ready.

The platform contract covers memory map and reserved regions, firmware-owned resources, interrupt/cache/MMU state, clocks, diagnostics, framebuffer/GOP, input, UFS Block I/O, EFI-stub/DT handoff and ExitBootServices. Bring up storage read-only before any write path. Preserve stock XBL, ABL and trusted firmware.

Dual boot and Fedora single-user-OS profiles both retain a recovery route and a verified return to stock. A/B slots do not isolate userdata. UEFI image identity, selected firmware profile and recovery state must be visible to the recovery manager. UKI/boot-manager integration follows proven EFI handoff.

EDK II platform code follows upstream C and assembly requirements. New native companion tools use C++; no Python is deployed to the tablet. Pin and document any unavoidable upstream host build tool separately.
