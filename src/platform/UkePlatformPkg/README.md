# Uke platform integration package

This is project-owned SM7675/Uke integration code, separate from immutable
Aloha donors. `BootDispatcher` links the real GPT/FAT32/config/image core and
uses firmware `LoadImage`/`StartImage` for the selected AArch64 manager.
The [12 October discovery update](../../../docs/ESP-MANAGER-DEVELOPMENT.md)
requires a type-valid partition named `ESP`, prioritizes `EFI/BOOT/BOOTAA64.EFI`
and otherwise discovers a unique installed ARM64 EFI application without
requiring `BOOT.CFG`. The provider receives that original installed path.
`Include/Protocol/UkeBootPlatform.h` defines the missing hardware provider's
entry, admitted internal storage, device-path, clock and Android-return ABI.
It does not install a pretend provider or borrow SM8150/SM8550 addresses.

The [6 October platform development](../../../docs/PLATFORM-DEVELOPMENT.md)
adds compiled SEC capture/admission, PrePi owned-memory planning, a native
UFS/SCSI read adapter, reverse driver shutdown, actual boot-services exit and
the final raw Android register/branch leaf. These libraries require real Uke
ownership, UFS initialization and architecture restoration backends. They do
not close the missing full firmware target. See the build/input manifest and
separate host/mock/QEMU receipts before integration.

The dispatcher requires a ready ESP-independent Android backend **before any
storage discovery**. Invalid or absent named ESP/image, ambiguity, a load
authentication error or a returned manager invokes that backend once. No reset,
EFI-variable flag or donor pstore address is used. A returned Android callback
is an error; it cannot be reported as a successful Android boot.

The application compiles with retained Mu MdePkg headers. It needs running
EFI services and the real platform protocol provider, so its PE binary is
neither SEC firmware nor a Nabu-style kernel-entry payload. Missing provider
returns `EFI_NOT_READY`. Firmware entry must not be enabled before the actual
provider and return route exist; this return value does not resume stock ABL.

Remaining raw firmware implementation is substantive, not empty file creation:

| Layer | Required implementation/input |
|---|---|
| SEC/PrePi and FD layout | Exact OS3 entry EL/cache/MMU state, authenticated ownership ranges, saved kernel/FDT/initrd/stack and permitted firmware/heap/stack placement |
| Platform memory/config libraries | Uke-derived complete ownership and MMIO/interrupt/clock contracts; no fixed donor memory map |
| Internal UFS provider | SM7675 controller **and PHY** initialization, reset/clock/power/SMMU, geometry/identity and bounded read operations |
| Android return provider | Driver-stage quiescence, ExitBootServices/state restoration and original-kernel continuation without ESP or reset loop |
| GOP and input | Measured inherited framebuffer or native display, Uke PMIC button wiring, console protocols and USB host/PHY backend |
| Linux handoff | Admitted Uke DTB/EFI-stub ABI and memory-map/ExitBootServices verification |

The OEM `uke-v-oss` reserved-memory source at
`94c84aeb9e517dbde546fc732033c184094c8e57` marks ADSP/SLPI at
`[0x9d400000, 0xa1900000)`. Nabu's `0x9fc00000` destination is inside that
range. This Android 15 source is a concrete reason to reject direct Nabu
placement; it does not establish OS3's complete runtime memory map.

`build-efi-dispatcher.sh` compiles only this integration application. No full
Uke DSC/FDF or hardware DXE set is asserted. HDMI mirroring and Windows remain
the final later milestones.
