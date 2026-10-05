# Project Aloha for POCO Pad X1 and Xiaomi Pad 7

A Uke-specific UEFI platform targeting POCO Pad X1 and Xiaomi Pad 7 (`uke`, SM7675). It is intended to connect a controlled recovery workflow to mainline Linux and Fedora while retaining a verifiable route back to Android for each model and firmware profile. Neither model has been boot-tested with project UEFI.

[Uke Linux](https://github.com/MCC45TR/uke-linux) · [Platform architecture](docs/ARCHITECTURE.md) · [Boot roadmap](https://github.com/MCC45TR/uke-linux-docs/blob/main/PLAN.md) · [Releases](https://github.com/MCC45TR/uke-project-aloha/releases)

## Platform goals

- Firmware-aware memory ownership, early diagnostics and display output.
- Input and read-only UFS access before any storage write path.
- Correct EFI-to-Linux device-tree handoff and ExitBootServices behavior.
- Explicit Android return, recovery access and selectable Fedora boot profiles.

The upstream Aloha platform tree does not currently provide a Uke target in the revision inspected for this project. A real SM7675 platform implementation is needed; another tablet's firmware image cannot be relabeled for Uke.

## Downloads

**No UEFI image is available yet.** Uke firmware, recovery and boot-path analysis come first. A future candidate will include its source revision, firmware requirements, hashes, known limitations and tested Android-return procedure. Physical boot results will be recorded in the [device status](https://github.com/MCC45TR/uke-linux-docs/blob/main/DEVICE-STATUS.md).

Upstream EDK II code retains its C and assembly conventions. New native companion utilities target C++; no Python runs on the tablet. Read [AGENTS.md](AGENTS.md) and the source licenses before contributing.
