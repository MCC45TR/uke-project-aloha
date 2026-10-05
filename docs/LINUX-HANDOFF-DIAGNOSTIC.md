# Linux EFI handoff diagnostic

`UkeLinuxHandoffProbe.efi` is an ARM64 EFI application that inspects a firmware
memory-map snapshot, reports the presence of one FDT configuration table and
prints GOP geometry when available. It returns to its caller. It is not a Uke
firmware volume, Linux loader or accepted handoff implementation.

The portable C memory checker accepts the standard little-endian EFI descriptor
ABI with explicit descriptor size/version and bounded count. It rejects page
alignment errors, zero-length descriptors, arithmetic overflow and overlapping
physical ranges, and inventories conventional RAM and runtime descriptors.
Its C++ host fixtures exercise malformed and adjacent ranges without modifying
input bytes. Unknown board addresses or carveouts are never synthesized.

The diagnostic allocates only its temporary map buffer and retries map growth
at most four times. It does not call `ExitBootServices`, install an FDT, load an
OS, set EFI variables or access storage. It cannot validate FDT ownership,
reserved-memory coverage, exact-profile RAM, framebuffer retention or UFS.

Linux 7.2.9's existing EFI stub obtains the final memory map and writes its
`linux,uefi-mmap-*` FDT properties while exiting boot services. A handoff probe's
earlier snapshot cannot replace that protocol. Firmware must classify its
carveouts correctly; a RAM-size SKU is insufficient evidence.

## Standalone host build

```sh
bash scripts/build-linux-handoff.sh /path/to/clang-r547379/bin /path/to/MdePkg
```

The supplied MdePkg headers are build inputs, never executed donor scripts.
The standalone build follows upstream UEFI C requirements and emits ARM64 COFF
PE32+ with EFI application subsystem, no import table and a zero timestamp.
It runs the native C++ fixtures first. Output and exact dependency logs stay
under ignored `build/linux-handoff/`.

The initial Clang 20 build and repeat produced SHA-256
`76457aecae3f2c6fbbe5b110e62cb48c859df1ee1dd608a7f1202e93a7979d4b`.
Host fixtures passed, including a Clang ASan/UBSan run. GCC sanitizers could not
link in this host snapshot because its ASan runtime was absent; the existing
Clang runtime was used, with C++ vptr instrumentation disabled as documented
for the core tests. No EFI execution, QEMU firmware test or Uke device test
was performed. Detailed dated lessons belong to private engineering records.

The optional INF is for integration with the separate Uke package descriptor.
The standalone build does not depend on the boot-manager core or that package
being installed in an EDK II workspace. Actual firmware entry, UFS backend,
Android return and exact-profile admission remain independent gates.
