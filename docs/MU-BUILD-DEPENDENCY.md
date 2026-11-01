# Host-only Mu build dependency

The Uke integration DSC uses the unchanged EDK II `BaseTools` build front end
from Mu BaseCore **562c0927ef7bfc65d26d9caf322e569ccb23b6a9**, the gitlink in
the reviewed Project-Aloha main revision. This upstream front end is written
in Python and generates the module build files, AutoGen C/headers and library
constructor/GUID bindings from INF/DEC/DSC metadata. A hand-written linker build
cannot validate those bindings; replacing BaseTools would duplicate the upstream
build system. It is therefore an unavoidable **host-only** build dependency for
the actual Mu integration check.

Only the host's `/usr/bin/python3.13` is selected, with its exact interpreter
version and SHA-256 recorded by the build script. Project-owned automation stays
in Bash. BaseTools source is imported from the checked archive into
`src/upstream/`; tools are compiled in an ignored build copy, leaving source
imports and reference repositories unchanged. No donor `build_uefi.py`, Stuart
bootstrap, pip install or donor device script is executed. Any missing upstream
Python module is resolved and recorded on the host, never copied to a payload.

Python, BaseTools and their dependencies are excluded from tablet applications,
initramfs, recovery, test images and the development ZIP. The Uke DSC integration
compiles existing EFI applications and platform libraries. It does not by itself
produce a bootable SEC/PrePi/DXE firmware or establish hardware acceptance.

The recorded host interpreter is Python **3.13.16**, SHA-256
`60414e94d7722fbdaedac9bf1f576118828305011e419329f56db7d13e49fef2`.
Native BaseTools need UUID headers/link inputs; the signed Fedora Rawhide
`libuuid-devel-2.42.4-2.fc46.x86_64.rpm` is extracted only into the ignored host
build sysroot (SHA-256
`10cdaaef2d196edc3f56ef2311f92cf28cb3b50972da12689be4ada3c8f2d8f5`).
The matching installed UUID runtime is copied into that sysroot; its hash is in
the build receipt. No host package installation is performed by this script.
GCC 16 requires `-Wno-error=unused-but-set-variable` only for native BaseTools.
The firmware sources keep their own warning-as-error policy.

MdePkg's package include list requires the exact MIPI Sys-T gitlink
`370b5944c046bab043dd8b133727b2135af7747a`. Its checked source import is connected
only in the working Mu copy; the trace library is not linked. The native tool
build selects GenFw/GenSec/GenFfs/GenFv and their dependencies; unused Brotli
tools and unrelated submodules are not built. This is selected component closure,
not a complete recursive firmware archive.

The upstream RELEASE rules also attempt to copy an optional PDB that this link
does not generate; make explicitly ignores that copy failure. The produced EFI
image passes PE/import inspection and actual QEMU `StartImage`. This nonfatal
debug-file limitation is retained in the private build log and scoped receipt.
