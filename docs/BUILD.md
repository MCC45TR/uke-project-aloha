# Building and checking the initial core

For installed-manager discovery on a partition named `ESP`, use the
[12 October ESP development guide](ESP-MANAGER-DEVELOPMENT.md). Its current
tests include VFAT long names and EFI service launch/fallback mocks; the
historical counts and source locks below are preserved.

For the current raw SEC entry probe, latest pinned Mu headers and the offline
test ZIP, follow [SEC probe development](SEC-PROBE-DEVELOPMENT.md). The commands
and fixed test counts below describe the earlier diagnostic stage. The dated
evidence recorders require their historical source revisions; current counts,
imports and the separate Mu integration build are in the SEC guide and receipt.

For the new boot v4 codec, direct Android continuation, AArch64 tool and Uke
dispatcher, use [the OS3 entry development guide](KERNEL-ENTRY-DEVELOPMENT.md).
The commands below retain the earlier core/diagnostic receipt. They do not build
raw Uke firmware or a Magisk/recovery installer.

SEC entry, PrePi memory, UFS read-only and EFI Android-return library commands
and their scoped receipts are in [platform development](PLATFORM-DEVELOPMENT.md).

Run these commands from the component root. Outputs stay under ignored `build/`.
No project-owned Python tool or tablet Python runtime is used. CMake/Ninja,
a C11/C++20 compiler and Bash are host tools. The disk round trips also need
`mkfs.fat`, mtools, `rg`, `sha256sum`, `truncate` and `dd`; the source/input audits
use Git, jq and GNU coreutils.

## Native host checks

```sh
cmake -S . -B build/host -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/host -j2
ctest --test-dir build/host --output-on-failure
```

`uke-esp-inspect` accepts read-only regular files and rejects final-path symlinks,
block/character devices, FIFOs and directories. It does not mount, repair,
dispatch or modify the image. Examples for disposable fixtures:

```sh
build/host/uke-esp-inspect gpt FIXTURE.img 512
build/host/uke-esp-inspect boot FIXTURE.img 512 uke-fixture
build/host/uke-esp-inspect manager BOOT.CFG CANDIDATE.efi uke-fixture
```

The `boot` command returns zero for a reported policy result, including Android
fallback. Check `boot-policy` and `reason`; exit zero does not mean an OS booted.
Bad CLI input returns two. The standalone `gpt`/`manager` checks return one for
a failed preflight. Files must remain stable during inspection; this CLI is not
a trusted concurrent-media or installed-device admission service.

## Standalone AArch64 EFI diagnostic

Supply an already-provenanced Clang tool directory and MdePkg header directory:

```sh
aloha_clang_dir=/path/to/clang-r547379/bin
aloha_mde_pkg=referances/donors/aloha-platforms/MU_BASECORE/MdePkg
scripts/build-efi-probe.sh "$aloha_clang_dir" "$aloha_mde_pkg"
scripts/record-build-evidence.sh "$aloha_clang_dir" "$aloha_mde_pkg"
tests/esp-roundtrip.sh build/host/uke-esp-inspect build/host/uke-fixture-disk \
  build/efi-probe/UkeEspProbe.efi
scripts/check-arm64-core.sh "$aloha_clang_dir"
scripts/audit-source-closure.sh
```

The initial run reused the existing host-only Android Clang 20 `clang-r547379`
toolchain, rather than installing or modifying a global compiler. Its compiler
and linker hashes are recorded in the narrow build manifest. This dependency
has not been archived as a complete standalone Aloha toolchain.

The build compiles all five portable C modules as freestanding ARM64 COFF. It
links only the Block I/O GPT diagnostic and GPT module into `UkeEspProbe.efi`,
with subsystem EFI application, no import table and PE timestamp zero. The core
uses EDK `Base.h` types under `UKE_EDK2`; the INF/DEC metadata has not been built
through a full Mu/Stuart DSC. No donor build script is executed. A complete
upstream host build workflow, including any unavoidable Python build tool,
needs a separate pinned host-only dependency decision before execution.

The EFI application requires existing firmware Block I/O and console services.
It inventories at most 32 physical handles and limits each GPT backend to 512
sector reads. It never admits a handle for boot, loads a manager, implements
UFS, touches Uke MMIO, changes a slot or executes Android return. Read-count
bounds do not establish a hardware deadline; the future UFS backend must enforce
one. This is not an Android boot image or a Uke firmware volume.

The ARM64 smoke command needs `qemu-aarch64`. It runs a freestanding Linux-ABI
test ELF with Linux write/exit syscalls. It exercises portable core logic only;
it is not QEMU firmware, a boot-manager trial or a tablet payload.

## Sanitizers

```sh
cmake -S . -B build/sanitized -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER="$aloha_clang_dir/clang" \
  -DCMAKE_CXX_COMPILER="$aloha_clang_dir/clang++" \
  -DUKE_SANITIZE=ON -DUKE_SANITIZE_VPTR=OFF
cmake --build build/sanitized -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitized --output-on-failure
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  tests/esp-roundtrip.sh build/sanitized/uke-esp-inspect \
  build/sanitized/uke-fixture-disk build/efi-probe/UkeEspProbe.efi
```

The recorded host GCC attempt could not link because `libasan.so.8.0.0` was
absent. The existing Clang runtime linked ASan/UBSan after explicitly disabling
C++ vptr instrumentation, whose runtime symbols were unavailable. Address,
ordinary undefined-behavior and leak checks remain enabled. A host with the
complete vptr runtime can leave `UKE_SANITIZE_VPTR=ON`.
