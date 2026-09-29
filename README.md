# Project Aloha integration for Xiaomi Pad 7

Develop a Uke-specific UEFI platform with diagnostics, GOP, storage, Linux handoff and a verified return to Android.

**Status: preparation only.** No project image has been built or tested on a Pad 7. This repository is a component of [Uke Linux](https://github.com/MCC45TR/uke-linux); see its [100-step plan](https://github.com/MCC45TR/uke-linux/blob/main/PLAN.md) and [hardware ledger](https://github.com/MCC45TR/uke-linux/blob/main/DEVICE-STATUS.md).

## Next implementation work

Resolve pinned Aloha submodules and map SM7675 platform requirements after the recovery foundation is established.

## Layout

- `src/`: project code; large active upstream checkouts use ignored `src/upstream/`.
- `configs/`, `patches/`, `scripts/`, `tests/`: reviewed configuration, attributed patches, host helpers and test definitions.
- `docs/`, `manifests/`, `reports/`: architecture, source identities and reviewed evidence.
- `referances/`: local unmodified reference clones and Git bundles; see its README.
- `build/`, `artifacts/`: local generated output, excluded from source publication.

New native tablet tools use C++. Host automation prefers Bash; Python must never ship to or run on the tablet. Upstream kernel/firmware languages remain unchanged. Read [AGENTS.md](AGENTS.md) before contributing.

The source plan lists component-relative reference paths. The workspace owns acquisition and archive verification through `scripts/sources.sh`; clone the workspace with submodules to use that orchestration. Reference history and licensing are preserved independently of this repository. The MIT license covers original preparation material, not imported upstream code.
