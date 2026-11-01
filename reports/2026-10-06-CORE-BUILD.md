# Initial Aloha core rebuild: 6 October 2026

Record ID: **ALOHA-BUILD-20261006-01**. Recorded date uses Europe/Istanbul;
verification review was `2026-10-05T22:28:21Z` (6 October locally).

The current project source successfully builds as an optimized native host
application and as a standalone ARM64 EFI diagnostic. No source fix was needed.
The full Uke platform firmware, manager dispatch and Android-return backend
remain unimplemented; this record does not establish a tablet boot result.

| Artifact / check | Result |
|---|---|
| `build/release/uke-esp-inspect` | GCC 16.2.1 Release build, x86-64 Linux ELF, 35,192 bytes |
| Release CTest suites | 7/7 passed: GPT, policy, config, image, buttons, FAT32 and inspector boundary |
| Release FAT32/GPT round trips | 9/9 passed; image hashes unchanged during inspection |
| `build/efi-probe/UkeEspProbe.efi` | ARM64 PE32+ EFI application, 8,704 bytes, timestamp zero, no import table |
| EFI artifact comparison | Same SHA-256 as the 5 October initial-core diagnostic |
| `build/arm64-fixture/uke-core-fixture` | Static ARM64 Linux-ABI test ELF; SHA/CRC/config/policy/buttons passed under QEMU user mode |

SHA-256 values:

```text
cf634faf9a863dd29c078e0c116b526438a1b907c70e784af504de109d3a827f  UkeEspProbe.efi
b1c141e005dc6f15bd2dcba1d0a4663c46227ffd18f4a4306053b23ccc9610d8  uke-esp-inspect
3e40da33ab8a406a9ce890bdc29c6416abf72d117bb20e4e92d1dee31444cc78  uke-core-fixture
```

The existing Clang 20 `clang-r547379` compiler/linker hashes and MU_BASECORE
commit matched the previous narrow build inputs. Actual source/header/tool
hashes are preserved in the [new input record](../manifests/initial-core-build-inputs-2026-10-06.json).
Its `date_utc` is 5 October because the local 6 October rebuild occurred before
UTC midnight. The old input record is preserved.

The host build used a separate `build/release` directory, preserving prior
Debug and sanitizer outputs. The [build guide](../docs/BUILD.md) contains the
commands; use `-DCMAKE_BUILD_TYPE=Release` for this host configuration.
Sanitizers were not rerun in this rebuild; their previous receipts remain in
the [initial implementation record](../docs/IMPLEMENTATION-STATUS.md).

The EFI artifact relies on existing EFI console/Block I/O services and links
probe/GPT only. The FAT scenarios use this diagnostic under fixture manager
names; they do not run real rEFInd or systemd-boot. QEMU user mode executes a
Linux-ABI test program, not firmware. No tablet operation, installation,
partition change or physical capability promotion occurred.
