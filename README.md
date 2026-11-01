# Project Aloha for POCO Pad X1 and Xiaomi Pad 7

A Uke-specific UEFI platform targeting POCO Pad X1 and Xiaomi Pad 7 (`uke`, SM7675). It is intended to connect a controlled recovery workflow to mainline Linux and Fedora while retaining a verifiable route back to Android for each model and firmware profile. Neither model has been boot-tested with project UEFI.

[Uke Linux](https://github.com/MCC45TR/uke-linux) · [Platform architecture](docs/ARCHITECTURE.md) · [Boot roadmap](https://github.com/MCC45TR/uke-linux-docs/blob/main/PLAN.md) · [Releases](https://github.com/MCC45TR/uke-project-aloha/releases)

**12 October 2026:** the owner's stock-ABL Fedora path now boots and works.
The [new SEC entry test](docs/SEC-PROBE-DEVELOPMENT.md) builds a raw Uke probe
and an offline boot-patching development ZIP. It uses the runtime Uke framebuffer
reservation, draws a marker and resumes the original kernel. Release/sanitizer
checks and five privileged QEMU-system cases pass. The real Mu component DSC
also builds; two QEMU firmware cases exercise the actual EFI dispatcher safely.
The patched tablet image is untested.
This is the first physical entry test candidate, not complete UEFI or an
installable dual-boot release. The earlier dated receipts below remain historical.

**12 October, ESP discovery update:** the [shared manager probe](docs/ESP-MANAGER-DEVELOPMENT.md)
now searches a valid partition named `ESP`. It prioritizes `EFI/BOOT/BOOTAA64.EFI`,
then a unique installed AArch64 EFI application such as rEFInd or systemd-boot.
No project configuration or manager copy is required. Ambiguity and errors
request Android through the platform provider. Host file/service tests and Mu
component builds validate this code; the raw marker ZIP still does not run EFI
or managers, and the actual Uke provider remains incomplete.

The [Uke implementation plan](docs/UKE-PORT-PLAN.md) defines rEFInd and
systemd-boot compatibility, volume/power navigation, USB keyboard and mouse,
conditional touch support, Fedora/Linux boot and Android return. USB-C-to-HDMI
preboot mirroring is the final firmware feature, with Windows reserved for the
last future multiboot expansion. The planned startup policy discovers an eligible
ESP and otherwise returns to ordinary Android. Recovery will own the reviewed
userdata-to-Android/ESP/Linux/future-Windows migration. These are planned capabilities.

The [initial implementation](docs/IMPLEMENTATION-STATUS.md) now provides native
GPT/FAT32 ESP inspection, manager/profile preflight, default-Android decision
logic and debounced volume/power events. Seven host suites and nine real
FAT32/GPT file scenarios pass, including sanitizer checks. An AArch64 EFI
diagnostic compiles and portable ARM64 fixtures run under QEMU user mode.
These results do not establish Uke boot or physical input support. Follow the
[build guide](docs/BUILD.md) and [platform admission contract](docs/PLATFORM-CONTRACT.md).

The [OS3 entry development](docs/KERNEL-ENTRY-DEVELOPMENT.md) now adds a native
boot v4 patch/restoration tool built for x86_64 and AArch64, a position-independent
Android-only continuation and a [Uke platform dispatcher package](src/platform/UkePlatformPkg/README.md).
Ten host suites passed in the 6 October entry receipt, with actual OS3 file round trips,
ARM64 user-mode checks and EFI-service mocks. Raw Uke firmware, hardware drivers
and Android return after firmware remain unfinished; no installable ZIP exists.

The [new platform libraries](docs/PLATFORM-DEVELOPMENT.md) add SEC entry
capture/admission, PrePi memory planning, UFS read access through a native EFI
driver, reverse driver shutdown, boot-services exit and the final Android
register/branch continuation. They compile for ARM64; 11/11 Release/sanitizer
suites and separate EFI mock/QEMU-user checks pass. Uke hardware initialization
and state restoration are still required before a complete firmware build,
installable ZIP or COPR firmware upload.

## Platform goals

- Firmware-aware memory ownership, early diagnostics and display output.
- Input and read-only UFS access before any storage write path.
- Correct EFI-to-Linux device-tree handoff and ExitBootServices behavior.
- Explicit Android return, recovery access and selectable Fedora boot profiles.

The upstream Aloha platform tree does not currently provide a Uke target in the revision inspected for this project. A real SM7675 platform implementation is needed; another tablet's firmware image cannot be relabeled for Uke.

## Downloads

The local [SEC test ZIP guide](docs/SEC-PROBE-BUNDLE.md) describes the new
host boot-patching development bundle. It creates the first temporary RAM-test
image from the exact working Fedora input, or the selected offline OS3 input.
No OEM image is redistributed. This development ZIP is not the firmware installer.

**No UEFI image is available yet.** Uke firmware, recovery and boot-path analysis come first. A future candidate will include its source revision, firmware requirements, hashes, known limitations and tested Android-return procedure. Physical boot results will be recorded in the [device status](https://github.com/MCC45TR/uke-linux-docs/blob/main/DEVICE-STATUS.md).

The requested installation artifact is an [Android boot-patching ZIP](docs/BOOT-INSTALLER-CONTRACT.md),
using the Nabu installer flow as a method reference. The current EFI diagnostic
is an intermediate test application. No installable Uke ZIP has been built;
platform firmware, the firmware-entry selector and post-firmware Android return
are still missing. The new continuation-only selector preserves the direct
Android path for offline development; it does not start UEFI.

Upstream EDK II code retains its C and assembly conventions. New native companion utilities target C++; no Python runs on the tablet. Read [AGENTS.md](AGENTS.md) and the source licenses before contributing.
