# Project Aloha for POCO Pad X1 and Xiaomi Pad 7

UEFI platform development for POCO Pad X1 and Xiaomi Pad 7 (`uke`, SM7675).
The port targets Fedora/Linux boot and return to Android.

[Uke Linux](https://github.com/MCC45TR/uke-linux) · [Platform architecture](docs/ARCHITECTURE.md) · [Boot roadmap](https://github.com/MCC45TR/uke-linux-docs/blob/main/PLAN.md) · [Releases](https://github.com/MCC45TR/uke-project-aloha/releases)

## Status — 26 October 2026

No bootable Uke UEFI image is available; neither model has booted this port.
The [native source checkpoint](docs/IMPLEMENTATION-STATUS.md)
includes the C++ boot core, GPT/FAT32/ESP contracts and guarded EDK II adapters.
All 12 host tests pass; the optional SEC framebuffer suite lacks its pinned
libfdt input and was not run. Firmware handoff, storage writes and Android
return remain unqualified. Current Fedora tests use the stock bootloader at EL1.

## Remaining work

- Firmware-aware memory ownership, early diagnostics and display output.
- Input and read-only UFS access before any storage write path.
- Correct EFI-to-Linux device-tree handoff and ExitBootServices behavior.
- Explicit Android return, recovery access and selectable Fedora boot profiles.

## Downloads

There are no downloadable UEFI builds. A release requires source and firmware
versions, checksums, installation limits and a tested Android-return procedure.
Detailed boot tests are recorded in the private
[device status](https://github.com/MCC45TR/uke-linux-docs/blob/main/DEVICE-STATUS.md).

Upstream EDK II code retains its C and assembly conventions. New native companion utilities target C++; no Python runs on the tablet. Read [AGENTS.md](AGENTS.md) and the source licenses before contributing.
