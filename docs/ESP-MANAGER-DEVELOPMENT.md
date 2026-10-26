# Installed ESP manager discovery — 12 October 2026

Record: **ALOHA-ESP-20261012-01**. The requested behavior is to open the
installed boot manager on the partition named `ESP`, without requiring a
project-specific manager copy or configuration. The shared C probe now does
this for bounded FAT32 installations, and the EFI dispatcher forwards the
selected original path and image to firmware loading services.

The [policy](BOOT-POLICY.md) defines exact selection, bounds and failure
behavior. The [build receipt](../manifests/esp-manager-build-2026-10-12.json)
records reviewed working-tree inputs and artifact hashes. The canonical
[lessons](https://github.com/MCC45TR/uke-linux-docs/blob/main/docs/lessons/2026-10-12-UKE-ALOHA-ESP-MANAGERS.md)
and [test record](https://github.com/MCC45TR/uke-linux-docs/blob/main/docs/testing/ALOHA-ESP-MANAGERS-2026-10-12.md)
separate source, file tests, service mocks, actual generic EFI execution and
physical acceptance.

## Implemented behavior

GPT discovery requires a valid EFI System Partition type and the name `ESP`,
case insensitive. It keeps partition GUID/geometry validation and the optional
firmware-owned GUID binding. `EFI/BOOT/BOOTAA64.EFI` is preferred; if absent,
exactly one valid ARM64 EFI application under an immediate EFI vendor directory
is eligible. Names such as `refind_aa64.efi` and `systemd-bootaa64.efi` work
through validated VFAT long names. No manager-name whitelist is used.

Multiple valid applications require an explicit standard default entry;
otherwise discovery reports `ambiguous-manager`. A present invalid default
is not bypassed. Invalid GPT/FAT, unsupported geometry, missing files and
load/authentication errors request Android. Discovery never repairs, mounts,
copies, formats or writes storage. The optional old `EFI/UKE/BOOT.CFG` route
remains available only after no automatic application is found.

The dispatcher re-probes and compares the selected path and hash immediately
before loading. The provider supplies the complete exact-media/partition/file
device path. A manager return requests Android once; no second unload is issued
for an application that `StartImage` already unloaded. Snapshot hashing is
consistency checking, not signer authentication.

## Reproduce the checks

Run from the component root with the pinned host Clang and Mu BaseCore inputs
described in [the host dependency decision](MU-BUILD-DEPENDENCY.md). Host-only
upstream BaseTools Python remains pinned there; no Python is deployed.

```sh
cmake -S . -B build/esp-discovery-20261012 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/esp-discovery-20261012 -j4
ctest --test-dir build/esp-discovery-20261012 --output-on-failure

aloha_clang_dir=/path/to/clang-r547379/bin
aloha_mde_pkg=src/upstream/mu_basecore-562c0927ef7bfc65d26d9caf322e569ccb23b6a9/MdePkg
bash scripts/check-efi-dispatcher-host.sh "$aloha_clang_dir" "$aloha_mde_pkg"
bash scripts/check-arm64-core.sh "$aloha_clang_dir"
bash scripts/build-efi-dispatcher.sh "$aloha_clang_dir" "$aloha_mde_pkg"
bash scripts/build-mu-integration.sh "$aloha_clang_dir"

bash tests/esp-manager-roundtrip.sh build/esp-discovery-20261012/uke-esp-inspect \
  build/esp-discovery-20261012/uke-fixture-disk build/efi-probe/UkeEspProbe.efi \
  build/efi-dispatcher-host/efi-dispatch-tests
bash tests/esp-roundtrip.sh build/esp-discovery-20261012/uke-esp-inspect \
  build/esp-discovery-20261012/uke-fixture-disk build/efi-probe/UkeEspProbe.efi
bash scripts/check-mu-integration-system.sh localhost/uke-qemu-console-host:20261006
```

The EFI probe input is the existing diagnostic application, not an actual
rEFInd/systemd-boot binary. It exercises installed names and valid PE content.
The optional fourth round-trip argument uses actual dispatcher source with
host service mocks: each eligible path gets successful StartImage return,
authentication rejection, failed StartImage and device-path failure cases.
These callbacks do not execute the selected EFI image.

Repeat the host checks with Clang ASan/UBSan using `UKE_SANITIZE=ON` and
`UKE_SANITIZE_VPTR=OFF`, as documented in [the build guide](BUILD.md). The
current run passed 13/13 Release and sanitizer suites, 11/11 automatic file
scenarios and nine legacy scenarios in both builds. Five eligible installed
paths each passed four dispatcher launch/fallback mocks; the existing seven
lifecycle and six bounded-I/O mocks also pass. Malformed LFN ordinal, type,
reserved cluster, padding and checksum cases are included in the core suite.

The real Mu INF/DEC/DSC build and standalone AArch64 EFI build pass. Two generic
QEMU firmware cases still verify safe missing-provider refusal and the
missing-image negative control. They do not exercise Uke storage or run a
discovered manager. The previous Mu/standalone dispatcher build outputs were
checkpointed under ignored `build/esp-checkpoint-20261012` before replacement.

## Delivery and remaining acceptance

`build/efi-dispatcher/UkeBootDispatcher.efi` and the Mu-built EFI application
contain the new selection code. They require already-running EFI services and
the unimplemented real Uke platform provider. There is no new raw firmware,
device installer or COPR firmware package in this record.

The previously delivered `artifacts/uke-aloha-sec-probe-20261012.zip` is unchanged
(SHA-256 `5e89ddca832d2396a1594e8debf4af976f968bb941be6f439a0df4ae5af0b67d`).
Its marker/continuation payload does not start DXE or discover the ESP. Do not
replace that payload with this EFI application: their entry ABIs differ.

The tablet was not operated. Full Uke SEC/PrePi/DXE ownership, UFS controller
and PHY initialization, the actual device-path provider and post-firmware
Android restoration remain open. Real manager resources, UI/input and Linux
handoff require separate acceptance. This reader scans one vendor level and
restricted ASCII names; deeper/Unicode installations need `BOOTAA64.EFI` for
now. Installer readiness remains unavailable.
