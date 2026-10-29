# OS3 kernel entry and boot-container development

Record: **ALOHA-ENTRY-20261006-01**. Scope: source, build, regular-file package
analysis and Linux-ABI QEMU user-mode execution. No tablet operation occurred.
The owner selected Global `OS3.0.303.0.WOZMIXM` for development on 6 October.
Its [profile](../profiles/uke-global-os3-development.json) is separate from the
dated OS2 stock observation and is not admitted for physical installation.

## Implemented components

- Native C++ `uke-boot-tool` and boot v4 codec, built for the Fedora x86_64 host
  and as a static AArch64 Linux/musl executable. It accepts bounded regular
  files, rejects devices/FIFOs/symlinks and writes only new output files.
- Position-independent `AndroidContinuation.S`, with an exact 256-byte template.
  It preserves x0-x3 and SP, uses x16/x17 scratch, replays the recognized original
  EFI MZ instruction when needed, and branches into the original kernel path.
  It does not initialize firmware or inspect ESP, MMIO or pstore.
- Profile-hash-bound staging, changed-header bookkeeping, double-patch rejection,
  capacity/branch bounds, original-kernel body preservation and exact restoration
  using the retained complete stock backup and the exact template.
- The [Uke platform integration package](../src/platform/UkePlatformPkg/README.md)
  and EFI dispatcher now connect actual filesystem/image preflight to
  LoadImage/StartImage and an explicit platform Android callback.

The raw [Linux boot ABI](https://docs.kernel.org/arch/arm64/booting.html) supplies
FDT in x0 and reserved zeros in x1-x3. The continuation changes no stack, MMU,
cache, interrupt or exception-level state. It is a development baseline for
preserving that path, not a return backend after firmware has changed those
states. It patches the raw entry instructions; the modified Image is not an
EFI-stub/PE load target.

The injected template follows `max(kernel_size, image_size)` rounded to 4096,
so it does not occupy the original Image's BSS/effective extent. Both boot
kernel size and Linux image size are expanded to include it. Loaded ownership
and ABL placement remain unverified on Uke; this arithmetic does not prove them.

## AVB correction

Read-only inspection found **two embedded AVB0 objects in a 16 KiB trailer**
despite the boot v4 header's zero signature-size field. A separate outer vbmeta
object is located by the partition AVBf footer. The codec checks their structural
bounds, not cryptographic validity. Its initial canonical-body-only footer
check rejected this real image; explicit bounded parsing corrected that error.
See the [AOSP footer definition](https://android.googlesource.com/platform/external/avb/+/refs/heads/main/libavb/avb_footer.h).

Staging requires `--unsigned-development` and removes all those stale metadata
blocks. It does not forge a signature, update OEM vbmeta or claim compatibility
with a locked bootloader. Restoration reproduces the complete original bytes,
including all metadata, from the separately verified stock backup.

## Verification

| Receipt | Result | Limit |
|---|---|---|
| BOOT-HOST-01 | 73 codec checks; malformed/truncated headers, bounds, metadata, hashes, entry instructions, capacity, template changes, damaged restore and double patch | Synthetic file fixtures |
| BOOT-FILE-01 | Six regular-file access boundary cases | No block-device read/write |
| OS3-HOST-01 | Eight round-trip/rejection scenarios, exact original restoration and immutable source hash | Actual Global OS3 regular file, no device |
| BOOT-ARM64-01 | Same 73 checks plus eight OS3 scenarios under QEMU user mode; identical staged image hash | Static Linux ABI executable, not Android recovery acceptance |
| ENTRY-ARM64-01 | Branch-first and MZ/second-branch fixtures preserve x0-x3/SP and reach the mocked original continuation | User-mode instruction execution, not ABL/EL/cache/DMA validation |
| DISPATCH-HOST-01 | Seven lifecycle scenarios; return gate before verification, one launch/one continuation, failed authentication and returned-backend handling | Mock callbacks, no boot services |
| DISPATCH-COFF-01 | ARM64 EFI application, zero imports | Compilation only; no UFS provider or running UEFI/manager result |
| DISPATCH-MOCK-01 | Seven hosted EFI lifecycle and six bounded-I/O scenarios under ASan/UBSan/leak detection | Mock EFI allocation/protocol/clock/storage callbacks, not firmware |

The complete host suite now includes ten CTest suites. The AArch64 link initially
requested unavailable libgcc/libgcc_eh; explicit compiler-rt/libunwind selection
resolved the link. The static executable is stripped of debug and comment
sections, including the toolchain's embedded build path. It has no interpreter
or needed shared libraries. Runtime dependencies are LLVM libc++/libc++abi,
libunwind/compiler-rt and AOSP musl; no Python is linked or shipped.

Review corrected the dispatcher I/O guard to split valid multi-sector FAT
cluster reads into 4 KiB backend chunks. The host EFI mock proves an 8 KiB read
reaches both chunks and rejects bounds, backwards time, deadline, read-budget
exhaustion and hardware-read failure. Its first compilation hit Mu's 16-bit
wide-character assertion; adding the same `-fshort-wchar` requirement used by
the firmware compilation corrected the host fixture configuration.

## Reproduction

Use the retained Clang and musl sysroot via caller-provided paths:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
ctest --test-dir build/release --output-on-failure
bash scripts/check-android-continuation.sh "$CLANG_BIN_DIR"
bash scripts/build-arm64-boot-tool.sh "$CLANG_BIN_DIR" "$MUSL_SYSROOT"
bash scripts/build-efi-dispatcher.sh "$CLANG_BIN_DIR" "$MDEPKG_DIR"
bash scripts/check-efi-dispatcher-host.sh "$CLANG_BIN_DIR" "$MDEPKG_DIR"
bash tests/os3-boot-roundtrip.sh build/release/uke-boot-tool \
  build/android-continuation/AndroidContinuation.bin "$OS3_STOCK_BOOT"
```

No installable dualboot ZIP exists yet. Raw Uke SEC/PrePi/DXE, the hardware
platform provider and post-firmware Android return are still missing. The
continuation-only development image must not be called an Aloha firmware image
or supplied as a dualboot replacement. Actual installation also needs selected-slot
backup, interruption journal, bounded partition write and full readback.
