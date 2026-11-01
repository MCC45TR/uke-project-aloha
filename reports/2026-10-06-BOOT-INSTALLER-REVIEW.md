# 6 October 2026: Android boot installer delivery correction

Record ID: **ALOHA-INSTALLER-20261006-01**. Scope: primary-source inspection,
offline OEM image verification and read-only readiness checking.

The owner clarified that the expected deliverable was a Nabu-style Android
boot-patching installer ZIP. The earlier EFI/host binary delivery did not meet
that request. No installable Uke ZIP was produced in either build session.
The [delivery contract](../docs/BOOT-INSTALLER-CONTRACT.md) now makes this explicit.

## Reproduced source and image observations

- Nabu installer workflow parent: `4ff90833837f7cefab947371a091069109937430`.
- Installer gitlink: `d91f5b8c79f936f6effff5556be3692eb1fa530f`.
- Nabu boot-manager PStore patcher: `366a199d97c83212a4a9a3806221c23b3206a6f5`.
- The four retained installer text files match their immutable Git blobs.
  They are selected reference files, not a complete archived repository.
- Project-Aloha patcher main remains `36edd373e8d0ff2fd620832f0fc5108f7bcbc84f`;
  its complete parent tree has no Uke selector or SM7675 config.
- Aloha main remains `7c81a0c77fbf77bc28a305e1e9d075f53648f93b`; its complete
  parent tree has no Uke/SM7675 platform. Nested dependency bodies were not
  exhaustively reinspected in this review. All three API tree responses are
  untruncated. No alternate fork is ruled out by this bounded inventory.
- The existing Global OS3 OEM `boot.img` matches the previously recorded
  SHA-256 and contains the kernel after the 4096-byte header page. Read-only
  kernel-header inspection finds `MZ` and AArch64 Image magic. The generic
  ramdisk is separately in `init_boot.img` according to the reviewed v4 profile.
- OS3 package layout is distinct from the dated OS2 physical stock observation;
  no profile mismatch was silently admitted.

The [evidence manifest](../manifests/boot-installer-evidence-2026-10-06.json)
records pins and selected-file hashes. Reference scripts were neither changed
nor executed. Existing early firmware and OEM images remain unchanged.

## Why the current artifact cannot become the requested ZIP

Nabu packages a raw firmware payload and kernel-entry selection code, not just
an EFI application. The current `UkeEspProbe.efi` assumes already initialized
EFI services. It supplies neither a Uke firmware platform nor an Android kernel
entry selector. The portable policy's Android action likewise supplies no
platform return/reset backend. Placing the application in a ZIP or appending it
to a kernel would not implement those missing parts.

## Changes and checks

The README, implementation status and plan now identify the installer ZIP as the
requested end product. The new readiness manifest enumerates six missing code
and input prerequisites; its Bash checker returns 3 for the current missing
prerequisites and 2 for an invalid CLI argument. Both outcomes were checked.
The checker has no packaging, patching or device access path.

JSON, shell syntax, local documentation links, privacy and whitespace checks
cover these new records. No native core source changed, so its existing build
receipts were not repeated or promoted. No fake installable ZIP was generated.

The web reader could not open the Git-tree API URLs. Bounded direct HTTPS
requests obtained and validated the immutable trees/raw files instead. Old local
Nabu build directories referenced by memory are absent on this host; the pinned
upstream source was inspected rather than treating old artifact paths as live.

## Next implementation

Implement the exact-profile raw Uke firmware and ARM64 entry/Android-return
contract, then the native image patch/repack/removal pipeline and bounded
installer backup/write/readback backend. Only then construct and verify the
alpha ZIP. An untested alpha can be honestly built before physical acceptance;
missing physical acceptance is not the reason the present implementation fails
to produce a ZIP. Actual platform code is missing.

No device unlock, reboot, flash, mount, slot selection or partition change was
performed. This review creates no physical acceptance result.
