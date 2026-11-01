# ESP manager discovery and input policy

Updated 12 October 2026. The portable C core, C++ host inspector and EFI
dispatcher share the same read-only manager probe. The inspector makes a
decision only; the dispatcher passes the selected image and its original ESP
path to firmware `LoadImage`/`StartImage`. That application still requires
running EFI services and a real Uke storage/Android-return provider. The raw
SEC marker ZIP has neither. See the [current build record](ESP-MANAGER-DEVELOPMENT.md)
and the historical [initial record](IMPLEMENTATION-STATUS.md).

## Discovery and failure behavior

The default action is `UKE_BOOT_ANDROID`. Only profile-admitted internal media
may participate in discovery. The host inspector admits a supplied regular-file
fixture; this is not a hardware identity check or UFS admission mechanism.
USB/external media are excluded by the policy, even if they contain an ESP.

GPT validation requires a protective MBR, primary and backup header CRCs,
reciprocal header positions, matching disk identity and partition arrays,
array CRCs, unique nonzero partition GUIDs, usable-range bounds and no overlap.
The parser supports 512–4096-byte power-of-two sectors, at most 128 entries of
128 bytes and at most 16 ESPs. Unsupported geometry falls back. No table is
repaired. A failed scan exposes zero candidates, including failures discovered
after an otherwise plausible ESP entry.

Candidates must have both the EFI System Partition type GUID and the GPT name
`ESP` (ASCII case insensitive, UTF-16 with zero padding). A same-named non-ESP
partition and an unnamed ESP are excluded. The name is a discovery filter;
the validated partition GUID and geometry remain its identity. The parser
publishes the name mask only after the complete GPT scan succeeds.

The policy selects exactly one **verified eligible** named ESP across admitted
media. Two eligible candidates are ambiguous. Invalid GPT on an admitted medium
aborts discovery. An optional partition GUID binding prevents falling through
to another ESP when the bound partition is missing or invalid; duplicate
bindings are rejected. Enumeration order never chooses a winner.

The verifier establishes filesystem bounds, a bounded installed path, AArch64
EFI application structure and a SHA-256 snapshot. The native host backend
performs these checks against actual GPT/FAT32 regular-file images.
Its FAT32 reader validates the BPB, capacity and root cluster, compares the
configured backup BPB and accessed mirrored FAT entries, bounds directory
traversal and rejects malformed VFAT metadata, duplicate selected names,
truncated or overlong file chains. It is a restricted reader, not a filesystem
repair or full fsck tool.
Directory traversal is limited to 128 clusters and 4096 entries per lookup;
manager files are limited to 32 MiB and configuration to 512 bytes.

| Condition | Policy result |
|---|---|
| No named ESP or missing bound GUID | Android requested |
| Unreadable/unsupported/invalid admitted GPT | Android requested |
| No candidate passes FAT/path/image checks | Android requested |
| Multiple eligible ESPs, ambiguous manager entries or duplicate binding | Android requested |
| Exactly one eligible candidate | Selected manager is eligible |
| Manager returns to its caller | Android requested |

Eligibility is not execution. The EFI dispatcher implements `LoadImage` /
`StartImage`, respects authentication rejection and requests Android once on a
load failure or manager return. It re-probes before loading and compares the
selected path and snapshot hash. The provider must create the full device path
for the exact media, GPT partition and installed file so the manager can find
its resources. A returned EFI application is [already unloaded](https://uefi.org/specs/UEFI/2.11/02_Overview.html) by `StartImage`;
the dispatcher does not unload that handle a second time.

A hung manager does not return through this function. Uke still needs an
ESP-independent Android backend and its real storage provider. The backend
must resume an established stock path or consume a proven one-shot bypass
before reset; an unconditional reset can loop into Aloha. Generic EFI refusal
or a host callback is not proof of Android return.

## Installed manager selection

1. Try `EFI/BOOT/BOOTAA64.EFI`. A present but invalid/unreadable default returns
   an error; it is never silently bypassed.
2. If the default is absent, scan one directory level below `EFI` for `.efi`
   files. Accept exactly one structurally valid AArch64 EFI application. This
   includes installed rEFInd, systemd-boot and other applications without a
   manager-name whitelist. Reject two valid entries as `ambiguous-manager`.
3. If none is found, try the optional older `EFI/UKE/BOOT.CFG` contract below.

The default name follows the [UEFI AArch64 fallback convention](https://uefi.org/specs/UEFI/2.11/03_Boot_Manager.html).
Prioritizing it on an internal named ESP is this project's policy choice.
The scanner cannot identify an application's purpose from its PE header: an
EFI utility or UKI can also be an application. Use `BOOTAA64.EFI` to designate
the intended manager when several applications share the ESP. Manager
configuration, resources and OS entries remain in their installed locations;
discovery copies or writes nothing.

The reader supports ASCII short names and checksum/ordinal-validated VFAT long
names up to 63 characters per component. Paths are limited to 127 characters,
exactly `EFI/vendor/file`, with letters, digits, `_`, `-`, `.`, and `~`; traversal,
backslashes and trailing dots are rejected. Discovery admits at most 32 vendor
directories and 1024 entries across those directories, in addition to the
per-directory bounds. Nested, Unicode or space-containing installations need
the standard default entry for now. `EFI/UKE` is reserved for the legacy
contract and is excluded from automatic vendor scanning.

Automatic entries require no `BOOT.CFG` or profile file on the ESP. The firmware
provider supplies the already-admitted profile. The snapshot hash detects byte
changes between selection and launch; it does not authenticate the supplier.

## Optional legacy configuration and manager copies

The legacy paths are `EFI/UKE/BOOT.CFG`, `EFI/UKE/REFIND.EFI` and
`EFI/UKE/SDBOOT.EFI`. The manager paths use fixed ASCII 8.3 names. Legacy installation
may place admitted copies of upstream AArch64 binaries there and must preserve
their resources, version and license information. No installed manager binary
is currently supplied. The round-trip fixture places the diagnostic application
under these names only to exercise the parser; it is not rEFInd or systemd-boot.

Configuration consists of exactly four nonempty ASCII `key=value` lines:

```ini
schema=1
profile=uke-p-global-os2.0.205.0.vozmixm
manager=refind
sha256=<64 lowercase hexadecimal characters for the complete manager file>
```

Only `refind` and `systemd-boot` are accepted. Unknown/duplicate keys, spaces,
CRLF, embedded NULs, arbitrary paths and unknown schemas are rejected. A final
newline is optional. The profile must exactly match the verifier's expected
profile. [The example](../config/boot.cfg.example) deliberately contains a zero
digest and is not an installable configuration.

A hash stored on the same writable ESP checks byte consistency; it does not
authenticate who supplied the loader. Profile text likewise does not prove
hardware identity. Firmware admission, trusted expected identity and actual
UEFI image authentication remain separate requirements. PE preflight checks
machine `AA64`, PE32+, EFI application subsystem, executable entry and section
bounds; it does not replace the firmware loader's relocation/security checks.

## Physical button state machine

The C state machine consumes a monotonic millisecond timestamp and sampled
volume-up, volume-down and power states. It emits previous, next and select
events. It does not read Uke GPIO/PMIC registers or install an EFI protocol.

- Changes require 30 ms of stable sampling.
- Volume emits one event, then repeats after 500 ms at 120 ms intervals.
- Power selects only on a debounced release of a press shorter than 800 ms.
- Keys held at initialization are suppressed until release.
- Chords suppress/disarm events; a backwards clock resets pending actions.
- Delayed polling emits no catch-up burst; timestamp addition saturates.

The next adapter must map events to each manager's input convention and combine
them with USB console input. A GPIO identity, electrical polarity, timer source,
EFI Text Input/Ex implementation and actual manager navigation require Uke
evidence. USB host/HID, mouse, native touch and display mirroring are still
separate unimplemented drivers and acceptance workloads.
