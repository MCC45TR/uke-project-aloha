# Uke Aloha SEC entry test bundle — 12 October 2026

This is the first raw, privileged Uke entry probe. It patches a **regular boot
image file on the host**, draws a green `UKE ALOHA LINUX` marker through the
bootloader's framebuffer when the retained Uke FDT is admitted, and resumes the
original kernel. The marker names the primary Fedora test; the optional OS3
input resumes its original Android kernel. A rejected FDT or unsupported CPU
entry state skips drawing and resumes the original kernel.

This is **not complete UEFI firmware, an operational dual-boot selector, a
Magisk module or a recovery installer**. It does not discover the ESP, execute
DXE, initialize UFS/USB/input, load rEFInd/systemd-boot or modify a partition.
No tablet operation has been performed with the patched images. Do not flash
this bundle or its generated images. Windows, touch and HDMI are later work.

## Create a local test image

Extract the ZIP into an owner-controlled directory. On the tested x86_64 Fedora
host, Bash, jq, GNU coreutils and the host C++/C runtime libraries are required.
No Python or tablet runtime dependency is included. The original boot image is
not redistributed; supply the exact input named by `profile.json`.

```sh
./make-sec-test-image.sh fedora-0242 /path/to/fedora_boot.img /path/to/uke-aloha-sec-fedora.img
```

The script checks the bundle, both input hashes, exclusive output creation,
readback and byte-exact reconstruction of the original image. It rejects a
different input or an existing output. SHA-256 binds the selected files; it does
not authenticate firmware signatures or establish installed-device compatibility.
The original remains unchanged. The result retains its 96 MiB container capacity.

`os3-global` selects only stock Global **OS3.0.303.0.WOZMIXM** and is an offline
development profile. It does not describe the installed Android 17 firmware.
Do not use that Android output on a different firmware profile.

## First physical acceptance, when the tablet is available

Use the existing stock bootloader/fastboot recovery route and retain the original
working Fedora RAM-boot image. For the qualified Fedora pair, the only candidate
operation in this test is a temporary RAM boot:

```sh
fastboot boot /path/to/uke-aloha-sec-fedora.img
```

Record the bootloader entry mode and a video of the display. Success requires
both the distinct marker and the original Fedora local console plus the existing
USB SSH route. No marker followed by working Fedora means bypass or unavailable
display evidence; it does not prove SEC display success. A hang/fault is a failed
trial: return to the bootloader through the existing power-button recovery route
and RAM-boot the original Fedora image. Preserve the selected stock slot and the
ESP/Linux/userdata layout. Do not rewrite the root filesystem for this test.

The marker may be brief or obscured by the subsequent console. The probe does
not install a persistent log, alter timer/interrupt/controller state, or provide
a watchdog/reset recovery backend. MMU and data cache must already be off,
interrupts masked, and the original raw Linux boot ABI must apply at EL1 or EL2.
The initial FDT pointer still relies on the bootloader's valid-pointer contract;
the probe cannot recover from an unreadable pointer without exception handling.

## What the host results establish

- Release and ASan/UBSan host suites: 12/12 each; codec: 93 checks.
- Read-only framebuffer admission: 37 synthetic checks and the retained original
  Uke FDT; its raw contents are not in the bundle.
- QEMU-system: branch and MZ kernel entries at EL1/EL2, plus a foreign-FDT bypass,
  checking framebuffer effects and original x0–x3/SP continuation: 5/5.
- Native and static ARM64 boot tools built; CLI rejects unsafe file inputs and
  output replacement. Local Fedora and OS3 files are independently restored.
- Separately, the actual Mu component DSC builds; two QEMU firmware cases check
  the EFI dispatcher's refusal without a Uke provider and a missing-image control.
  These EFI components are not embedded in this marker probe.

These are host/build/emulation results. They do not establish a physical Uke
probe boot, full EFI services, Android return after hardware reinitialization,
or an installable Aloha release. This bundle is not eligible for a COPR firmware
package. Project-Aloha Mu headers, libfdt and the original project code retain
their own licenses; the marker payload includes the BSD-licensed libfdt reader.
