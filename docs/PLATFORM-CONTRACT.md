# Uke platform admission contract

The tables below retain the initial 5 October preparation state. Later partial
entry/Mu work is recorded in [SEC probe development](SEC-PROBE-DEVELOPMENT.md);
the [12 October ESP policy](BOOT-POLICY.md) now discovers installed managers
without mandatory `BOOT.CFG`. Neither update establishes a complete Uke
hardware provider or post-firmware Android return.

The [initial profile](../profiles/poco-pad-x1-global-os2.json) is a preparation
record for the dated POCO Pad X1 Global stock observation. It cannot authorize
execution. Xiaomi Pad 7, other SKU/firmware combinations and the OS3 recovery
candidate do not inherit its identity or admission.

## Evidence retained and unresolved inputs

The public [stock summary](https://github.com/MCC45TR/uke-linux-docs/blob/main/docs/research/UKE-HARDWARE-STATUS-2026-10-04.md)
records POCO Pad X1 `25099RP08G`, `uke_p_global`, SM7675, 8 GB / 512 GB and
`OS2.0.205.0.VOZMIXM` on 3 October. The stock boot-state observation was
locked/green; it is not a live observation on 5 October. Raw geometry and
protected memory contents were inaccessible. Unit identifiers and raw captures
are excluded from this component.

| Required input | Current state | Implementation consequence |
|---|---|---|
| Profile-matched Aloha entry ABI and container | Unknown | No Uke SEC/PrePi/FDF or executable entry recipe |
| Stock XBL/ABL/trusted-firmware ownership | Retain stock; handoff unproven | No replacement of early security firmware |
| RAM ranges, protected carveouts, stack/FV placement | Unknown | No invented physical load addresses |
| Exception level, MMU/cache/SMMU/interrupt state | Unknown at candidate entry | No assumption that stock services remain callable |
| Console, timer, exception and reset escape | No Uke backend | Portable key timing tests do not establish a timer |
| Internal framebuffer ownership and handoff | Stock/source leads only | No GOP claim |
| UFS LU/device identity, sector size and GPT | Protected stock reads unresolved | Read-only fixture support is not a UFS driver |
| ESP-independent normal Android return | Unknown | No dispatcher may be admitted for a physical trial |
| Recovery entry and stock restoration prerequisites | Separate recovery gate | No storage migration recipe or trial admission |

The fields remain explicit `null` in the preparation profile. A future profile
must attach exact firmware/artifact hashes and evidence receipts before turning
these into operational values. Never import a Nabu/Sheng/Pipa FDF address,
partition offset, GPT backup or security binary as an Uke fact.

## Entry and return are one decision

Before choosing a secondary Android payload, patched handoff or another proven
entry method, identify what happens when no ESP exists. Merely returning from
an EFI application can return to a shell or manager; it does not necessarily
resume stock ABL. Merely storing an Android-return EFI application on the ESP
fails the absent-ESP requirement.

The admitted design must establish the stock continuation or one-shot bypass,
its storage/reset behavior and its bounded failure path. It must preserve
recovery access and avoid a recurring reset into the same dispatcher. Physical
validation includes no ESP, damaged GPT/FAT, wrong profile, missing/invalid
manager, returned manager and repeated cold/warm boot. All remain open.

## Source baseline boundary

The retained parent reference is Aloha
`8d37e2bfab2f9d959ac0b3d68434e8be4866bf55`. The initial diagnostic uses headers
from its MU_BASECORE gitlink
`762a65dad79e2934cddbefa467b7e2c7b5ee4bc0`. This narrow build does not adopt the
separately researched newer Aloha parent as a development baseline.

The [checkout identity audit](../manifests/source-closure-2026-10-05.json)
contains 44 visited nodes: 32 exact checkouts, including the root, and 12 missing
nested checkouts. It records identity only. Dirty files, complete recursive
history, unvisited descendants of missing nodes, binary payload provenance,
license review and offline restore are not established. The broader archive
ledger remains incomplete. Source acquisition remains under `referances/`;
project implementation remains under `src/`.

The [build-input record](../manifests/initial-core-build-inputs-2026-10-05.json)
hashes actual compiler-discovered headers, project sources and the two tools
used for the standalone diagnostic. It is not a full platform lock or SBOM.
