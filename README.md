# Project Aloha for Uke

UEFI platform development for POCO Pad X1 and Xiaomi Pad 7 (`uke`, Qualcomm SM7675). The intended boot path covers Linux and a controlled return to Android.

**Source status updated 26 October 2026. No bootable Uke UEFI release is available.**

[Uke Linux](https://github.com/MCC45TR/uke-linux) · [Architecture](docs/ARCHITECTURE.md) · [Source checkpoint](https://github.com/MCC45TR/uke-project-aloha/tree/publish/source-snapshot-20261026) · [Releases](https://github.com/MCC45TR/uke-project-aloha/releases)

## Current status

Current Fedora tablet testing uses the stock Android bootloader. Neither model has booted this UEFI port. Host tools, EFI components and platform source are separate from a complete Uke firmware implementation.

The [26 October source checkpoint](https://github.com/MCC45TR/uke-project-aloha/tree/publish/source-snapshot-20261026) publishes the native GPT/FAT32/ESP core, boot-image tools, EFI dispatcher and guarded platform adapters. Its Release host CMake/CTest run passes 12/12 tests. The optional SEC framebuffer test needs the separately prepared pinned libfdt source and was not run in that checkout.

| Area | Source / test result | Remaining gate |
|---|---|---|
| GPT, FAT32 and manager selection | Native host parser, policy and file-boundary fixtures | Actual UFS controller, firmware filesystem and selected manager execution. |
| Boot-image inspection | Native C++ tooling and host tests | Complete platform entry and physical handoff. |
| EFI dispatcher and Android return | Guarded component source and earlier component fixtures | Complete firmware integration, ownership restoration and physical return to Android. |
| SEC/PrePi/platform admission | Bounded memory, entry and framebuffer source | Exact firmware/memory contract and actual Uke startup. |
| EL2 handoff | Offline firmware/source research | Qualified stock-firmware service, policy, caller state and all-CPU runtime behavior. |

Host or generic VM success does not establish Uke firmware execution. No runtime SMC, firmware replacement, partition write or device reboot is performed by this source publication.

## Downloads and remaining work

There are no downloadable Uke UEFI images or accepted Android boot-patching installer ZIPs. A release requires complete platform source/dependency closure, exact firmware compatibility, checksums, validated input/storage/display behavior and a tested recovery/Android-return route.

Preserve stock early firmware. Source from another Qualcomm device is comparison evidence; board memory maps, GPT data, security binaries and calibration are not portable.

Upstream EDK II components retain their C/assembly conventions and licenses. Native companion utilities use C++. No Python runs on the tablet.

Read [AGENTS.md](AGENTS.md), the owning source licenses and the checkpoint's build/implementation documents before extending the port.
