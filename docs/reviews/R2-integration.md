# M1 integrated Hub — R2 follow-up

R2 updates `codex/m1-integration` from `797947f` for PR #8. Main and the lane branches remain unchanged. All verification in this pass uses synthetic data, synthetic child processes and Qt offscreen/software rendering. No real Steam files, owner library, emulator, SteamVR, headset, GPU timing or system/toolchain changes are used.

## Fixed

| R2 item | Result | Regression evidence |
|---|---|---|
| 1 | A completed rollback restores prior installed states and records the actual operation error. Runtime ownership remains visible. | Installed-state rows, runtime/GameRecord assertions and uninstall error-code regression in InstallTest. |
| 2 | Windows probes the portable OpenVR DLL and its required exports before calling OpenVR. Missing DLL errors are explicit; only user-started overlay runs open desktop fallback. Setup documentation matches. | A copied executable without the DLL exercises overlay, runtime-started overlay, registration and removal under offscreen/software settings; no OpenVR initialization occurs. |
| 3 | Machine BIOS choice is saved in scan bindings and passed as `-bios`. Old receipts without this selection require a new scan. Device BIOS requirements use the declared default. | BIOS alternative/default selection, binding serialization/restored-receipt validation, launch argument and stale-receipt tests. |
| 4 | Steam removal requires the exact stored game and variant pin; missing or mismatched pins are refused before writing. | Synthetic variant mismatch, legacy/unpinned, malformed-option and integration preview tests. |
| 5 | Explicit media IDs resolve the three affected recipes. Both validators reject unresolved requirements and malformed media contracts. | Shipped catalog recipe checks, resolution/type fixtures and Python policy tests. |
| 6 | Old-log deletion is advisory, last-played persistence runs off the GUI thread with immediate lock refusal, preflight logs rotate per game, Preparing/completion reserves launch lifetime, and launch-error presentation has a 15-second deadline. Reentrant completion cannot overwrite a replacement launch. | Rotation fault injection, held-lock responsiveness, completion/replacement lifecycle, log retention and synthetic modal-deadline tests. Native dialog acceptance is open below. |
| 7 | The manifest marks SteamVR-started instances; failed initialization exits those instances. User-started runs retain desktop fallback. | Origin-policy matrix and DLL-absent subprocess exit assertions; windowless fallback completion policy. |
| 8 | Missing original media links remain uninstall-incomplete; unrelated broken metadata does not disable valid plans; cross-check notices are informational; recovered envelopes preserve a good previous generation. User-media-copy removal uses reversible run-local quarantine and purges it after completion/recovery. | Missing-source, malformed-catalog, genuine-warning, envelope, media-copy and crash/recovery tests. |
| 9 | Merged clone CHDs resolve in parent folders with case-insensitive folder matching; header SHA remains indexed after sparse-read failure; disk-only sets cannot bind unrelated archives; requested sets win equal scores. Facet notifications coalesce. Validator required-field, media-resolution and warning-severity checks align. | CHD/merge/disk-only/header/tie fixtures, coalesced notification assertions and eight Python/C++ base-contract comparison rows. |
| 10 | Filter expansion survives recount by ID, recovery Retry keeps failed identity, card Retry keeps the selected identity, and Explore prefers portrait art. | Recount/expansion, failed-versus-open-page/card target and portrait-binding UI tests. |
| 11 | New Steam AppIds use a stored game disambiguator; existing owned AppIds and user fields remain preserved. | Same-title creation, rename/move, legacy persistence and collision tests. |
| 12 | Cover tests use synthetic identifiers. The public audit catches serial-form image names in code, receipt prose and operator headset names with synthetic look-alike regressions. Time Crisis II cites the pinned GameIndex URL with refreshed checked date and a serial-free route comment. | Privacy policy tests and tracked-text audit; provenance assertions; synthetic cover resolver test. |

## Verification

- Final Windows Release build succeeded with the existing MSVC/Qt toolchain.
- All 13 CTest suites passed with `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software` and `QT_OPENGL=software` (186.30 seconds). The opt-in GPU benchmark remained disabled.
- Public catalog: 413 records, zero errors and two existing source-metadata warnings. Six synthetic privacy regression tests and the tracked-text audit passed.
- Targeted Retry/expansion/portrait/coalescing and reentrant-completion/lock tests passed before the final combined run.
- Independent read-only source review found the additional completion/Retry integration edges; after fixes and tests it found no remaining concrete regression in those paths.
- Hosted Windows/Linux CI will separately verify the pushed commit; no CI result for the new commit is claimed in this source report.

## Deferred with reasons and open gates

- Python/C++ comparison covers common required-field, type/severity and recipe-media checks on eight base-catalog fixtures, plus identical shipped-catalog totals. Complete validator equivalence beyond those compared contracts, including arbitrary pack/user-override layering and further malformed-field edge cases, is deferred: Python has no corresponding layer loader and there is no single shared validation schema defining all those cases.
- Device BIOS alternatives that cannot be addressed independently by the launch CLI require the declared default; unsupported alternatives fail closed.
- Native Windows launch-error dialog dismissal is covered by the bounded presentation helper, but its actual window behavior remains unverified under the required offscreen test mode.
- S1–S3 runtime/compositor, headset laser/keyboard, overlay plus desktop, registration/removal and runtime-quit acceptance remain open under the no-GPU/no-SteamVR instruction.
- Real Scud Race, Time Crisis and Time Crisis II Hub/Steam launches, focus/exit behavior, real Steam entry/art acceptance, and grid/GPU timings remain open under the no-emulator/no-real-Steam/no-GPU instruction.
- R1's remaining ownership-generation repair/pruning, fresh optional release validation, non-PS2 automatic product indexing/GDI/CUE traversal, stronger PC identity, remaining synchronous GUI I/O and immediate config-only Save transaction remain deferred for the reasons recorded in [R1](R1-integration.md).

PRs #1–#7 remain superseded by PR #8, open and unmerged. No acceptance gate above is claimed by synthetic tests or green CI.
