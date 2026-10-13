# Project Aloha for POCO Pad X1 and Xiaomi Pad 7

UEFI platform development for POCO Pad X1 and Xiaomi Pad 7 (`uke`, SM7675).
The port targets Fedora/Linux boot and return to Android.

[Uke Linux](https://github.com/MCC45TR/uke-linux) · [Platform architecture](docs/ARCHITECTURE.md) · [Boot roadmap](https://github.com/MCC45TR/uke-linux-docs/blob/main/PLAN.md) · [Releases](https://github.com/MCC45TR/uke-project-aloha/releases)

## Status — 13 October 2026

No Uke UEFI image is available. Neither model has booted this port. The inspected
upstream Aloha revision has no Uke target; SM7675 platform support remains to be
implemented. Current Fedora tablet tests use the stock Android bootloader.

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
