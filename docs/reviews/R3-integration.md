# M1 R3 cleanup

R3 continues on `codex/m1-integration` because PR #8 was open at intake (base `8e85718`). No merge or main rewrite is part of this pass. Source, synthetic child processes and Qt offscreen/software tests are the permitted evidence; no real Steam, emulator, SteamVR, headset, GPU or owner-library activity is included.

## Fixed and regression coverage

| R3 item | Result | Synthetic regression |
|---|---|---|
| 1 | Header-only CHD evidence remains allowed for bounded unsupported sparse decoding. Map headers/bodies and a bounded metadata chain must fit the physical file; cycles and exhausted metadata budgets fail closed. Legacy file caches are reinspected; saved CHD bindings require provenance for every primary/support CHD before restore or launch. | Physical offset/range rows, metadata-chain/cycle cases, old-cache reinspection, bounded unsupported decoding, serialized provenance, saved legacy primary/support/renamed receipts, restore and flat launch rejection. |
| 2 | New Steam writes omit the unused disambiguator field; owned updates prune it while preserving existing AppIds and user fields. Section 5.3 documents the actual CRC suffix and ownership lookup. | Exact AppId formula, same-title/rename behavior, absence of obsolete field and retained user/ID data. |
| 3 | Windows DLL probing uses native separators. Probe-only API checks the shipped DLL and exports without initializing OpenVR. | DLL-present probe and existing copied-executable DLL-absent actions; no VR_Init call in the present test. |
| 4 | Quarantine purge checks its result and warns on failure. Startup retries only journal-proven finished removals off the GUI thread. Dropped copied media uses reversible quarantine. Completed rollback restores nontransient prior state; rename failure names quarantine correctly. | False-purge injection/retry, conservative startup fixtures plus Hub-startup integration, dropped-row commit/rollback, terminal-state rows, precise rename/collision error. |
| 5 | Last-played persistence uses a dedicated owned pool and a bounded 1500 ms worker lock wait. Launch errors reopen the closed desktop Hub and remain visible in status. | Transient lock handoff, sustained contention and GUI heartbeat, global-pool starvation, teardown, R2 reentrancy, hidden-window error/status integration. |
| 6 | Recovery Retry is visible/actionable only for an identified install operation; fallback error reason reaches desktop status. | Install/uninstall/repair/missing-ID/generic-failure game/tool visibility and action cases; fallback status reason assertion. |
| 7 | Malformed authored install TOML warns without disabling working generated flat routes. Non-table variant/needs/controls are reported by Python and C++. CHD folder matching folds case only on Windows. | Ready flat route/plan after broken recipe, malformed table-shape policy/parity rows, Windows/Linux folder expectations. |
| 8 | Ordinary image dimensions are not serial evidence; checked dates are validated without a fixed date; count-free positive receipt prose is audited. Quest 3 remains public project information and is restored in the requested docs. | Nine synthetic privacy cases including ordinary names, no-count receipts/clause negation, public headset model, serial forms, paths/hardware and pinned provenance. |

## Verification

- Final Windows Release build succeeded with the existing toolchain. All 13 GPU-free CTest suites passed across the combined run and corrected-suite rerun: the unchanged nine suites passed in the combined run, then UI, scan/art, integration and launch/Steam passed together after fixture corrections (162.18 seconds).
- Nine privacy regressions and the tracked-text audit passed. Four Python catalog-policy regressions passed; the shipped catalog reports 413 records, zero errors and two existing warnings. Twelve Python/C++ base-contract fixtures are compared by the catalog suite.
- The DLL-present export probe passed without OpenVR initialization; the DLL-absent subprocess tests passed in integration coverage.
- Independent source review confirmed metadata-chain bounds and enforcement at restore/launch; no remaining concrete bypass was found in that reviewed scope.

Tests use `QT_QPA_PLATFORM=offscreen`, `QT_QUICK_BACKEND=software`, `QT_OPENGL=software`; the opt-in GPU benchmark remains disabled. Hosted Windows/Linux CI for the pushed R3 head is reported in PR #8.

## Limits and open acceptance

- Header bounds and header identity are not a CHD payload/content audit. Metadata-header traversal is explicitly bounded; exceeding its budget requires further support rather than accepting unchecked evidence.
- Startup cleanup preserves interrupted, orphan, corrupt, linked and unrecorded runs. Only retained trustworthy terminal journals authorize deletion; older untrusted folders and previous install generations remain preserved.
- Python authoring lint continues to reject syntactically broken recipes. Runtime uses a warning so working generated flat routes remain usable. Broader arbitrary-value/layered validator equivalence remains outside the compared shape contracts.
- Privacy checks are heuristics with discriminating synthetic cases, not a guarantee that every possible private phrase is recognized.
- S1–S3 compositor/headset/input/registration/quit, real flat Hub/Steam launches, real Steam artwork acceptance, native dialog/focus acceptance and grid/GPU timings remain open under the no-GPU/no-real-runtime instruction.
- R1/R2's previously documented trusted-generation repair/pruning, optional release validation, identity/traversal, worker migration and immediate config-only Save gaps remain deferred for their recorded reasons.

PR #8 remains the integration PR while open. PRs #1–#7 remain superseded, open and unmerged.
