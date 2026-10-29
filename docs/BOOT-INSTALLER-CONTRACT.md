# Android boot patching ZIP delivery

The [12 October development ZIP](SEC-PROBE-BUNDLE.md) provides an offline raw
marker/continuation test, not the complete installer described here. The
[installed-manager discovery update](ESP-MANAGER-DEVELOPMENT.md) is compiled
into a separate EFI application; adding discovery does not make the marker
payload start UEFI or close the full installer prerequisites.

The requested deliverable is a **Magisk/recovery installer ZIP that patches the
Android kernel in `boot.img` to enter Aloha while preserving Android**. The
previous build reply supplied a diagnostic EFI application and host tool instead.
Those are intermediate components and do not satisfy the requested installer.

## Verified Nabu method

The [Nabu workflow](https://github.com/rodriguezst/nabu-dualboot-img/blob/4ff90833837f7cefab947371a091069109937430/.github/workflows/build.yml)
builds real Aloha firmware, an ARM64 kernel patcher and selection stub, then packs
them with a boot-image repacker and installer. Its installer gitlink is
`d91f5b8c79f936f6effff5556be3692eb1fa530f`. Boot-manager builds use the PStore
patcher fork, inspected at `366a199d97c83212a4a9a3806221c23b3206a6f5`.

The [entrypoint](https://github.com/rodriguezst/Flashable_UEFI_Installer/blob/d91f5b8c79f936f6effff5556be3692eb1fa530f/META-INF/com/google/android/update-binary)
reads a boot slot, backs it up, unpacks its boot image, patches the kernel with
raw firmware and a board-specific selector, repacks it and writes it back.
The reference script was not executed. Its broad temporary-directory cleanup,
overwritten backup and unchecked final write must not become the Uke design.

The [Nabu PStore selector](https://github.com/rodriguezst/DualBootKernelPatcher/blob/366a199d97c83212a4a9a3806221c23b3206a6f5/ShellCode/ShellCode.NabuPStore.S)
uses board-specific memory to choose Android or firmware. Its memory address,
magic value and SM8150 firmware placement cannot be assumed for Uke.
`UkeEspProbe.efi` needs already running EFI services; it cannot replace a raw
firmware payload that establishes those services after Android kernel entry.

## Required Uke pipeline

1. Bind the exact firmware profile and original boot-image hash.
2. Build Uke SEC/PrePi/DXE and its raw firmware payload using an established
   entry ABI, permitted RAM and protected-memory ownership contract.
3. Build the Uke ARM64 selector, preserving original Android continuation and
   registers. Firmware then performs the requested ESP discovery.
4. Implement ESP-independent Android return and one-shot/reset-loop prevention.
5. Build the native ARM64 patcher/remover and boot repacker. Verify preservation,
   reversal, bounds, capacity, headers and signature/AVB behavior on regular files.
6. Implement the installer profile/slot/update-state checks, durable original
   backup, interruption handling, bounded selected-slot write and full readback.

The eventual ZIP contains its `META-INF` entrypoint, native patcher/remover,
repacker, Uke selection stub/configuration, raw firmware, manifests, hashes,
license/source notices and installation/restore instructions. It must not bundle
an OEM boot image as a silent universal replacement. No tablet Python is used.

An alpha ZIP may be built before physical acceptance when its actual platform
implementation and offline checks exist. The present blocker is missing code
and inputs, not simply an uncompleted physical test. Building an alpha does not
authorize flashing it or promote a physical capability result.

## Profile and current status

The offline Global OS3 candidate has boot header v4: its kernel is in `boot.img`
and generic ramdisk in `init_boot.img`. Its boot hash matches the recovery catalog
`efdee1d4e1acd7f6e77615330dbcb045caeeccad8568d89fa6abd627f606e77f`.
Read-only inspection at kernel offset 4096 found AArch64 Image and EFI-stub
markers. This establishes layout, not Uke Aloha compatibility. Patching only
`init_boot` would not add the requested kernel-entry selector for this candidate.
Preserve separate init/vendor boot and DTBO plus the exact-profile AVB contract.

This candidate is `OS3.0.303.0.WOZMIXM`; the dated physical stock observation is
`OS2.0.205.0.VOZMIXM`. They cannot silently share patch inputs or admission.
Evidence is in the workspace recovery component's
`manifests/boot-profile-global.json` and `manifests/stock-boot-programming-global.json`.

The owner subsequently selected OS3 for offline development. The
[kernel entry development record](KERNEL-ENTRY-DEVELOPMENT.md) adds a native
ARM64 boot codec, reversible Android-only entry and the Uke dispatcher package.
The complete stock file round trip passes; OS2 admission remains separate.

No installable Uke boot-patching ZIP exists. The [readiness record](../manifests/boot-installer-readiness.json)
tracks six prerequisites: the OS3 input is established, while five remain
incomplete despite the new partial implementations. `scripts/check-boot-installer-readiness.sh`
returns 3 for missing prerequisites and 2 for invalid input. Exit zero means
only the prerequisite record is complete; this checker never packages or patches.
The [review record](../reports/2026-10-06-BOOT-INSTALLER-REVIEW.md) preserves the
exact upstream pins and the distinction from the previous diagnostic build.
