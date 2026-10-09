# M1 integrated Hub — R1 report

The integration branch combines lanes A–G and the R1 fixes requested in
`docs/tasks/m1/R1-review-fixes.md`. Main commit `767b114` was merged into the
integration branch first. Main history and the seven lane PRs are preserved.

This pass uses synthetic fixtures, synthetic child processes and Qt offscreen /
software rendering only. No SteamVR, emulator, headset, GPU timing, owner library
scan, real Steam write or system/toolchain change is part of this verification.
Historical private operator receipts do not close the current acceptance gates.

## Fixed and verified

| R1 items | Change | Regression evidence |
|---|---|---|
| 1–5 | Private scan receipts, drive paths, cover association and exact owner hardware removed from committed text; PR #5 description sanitized | Tracked-text privacy audit, four checker regressions, synthetic cover and path fixtures |
| 6–8 | Emulator install gate allow-list; catalog content policy required and validated; media paths/outputs and hard-link aliases rejected as extract/copy sources | All shipped manifest gate cases, missing/malformed policy cases, source-provenance and metadata-only hard-link alias cases in InstallTest |
| 9 | Faceted counts and three hardware tree levels | Offscreen UI tree/count/no-undefined assertions |
| 10 | Windows GUI subsystem; parent console only for CLI/diagnostics; portable failure logs and native Windows error display | PE subsystem check; synthetic Unicode/space launch preflight error creates portable log; native display is not visually accepted |
| 11–13 | Selected game retry distinct from failed tool recovery; busy feedback and Stop; separately approved Steam removal precedes owned payload uninstall | Shipped flat route refusal, game/tool retry, UI Stop signal, hung synthetic child Stop, missing account/malformed preview/running refusal and approved removal ordering |
| 14–16 | Existing Steam user fields preserved; shortcut pins variant; final VDF/art guards, no-op writes and permanent first backup | Synthetic Steam-shaped VDF, preview fields, variant CLI, race/no-op/backup regressions |
| 36 (host), 37–39 (parent portions) | 1280×800 overlay coordinates; initialization failure opens desktop; delayed OpenVR DLL; local-only QML networking; immutable dependency/action pins and package SHA; shared last-played lock and 20-log history per game | Fake runtime never allocates renderer on failure; software scene input; copied CLI without OpenVR DLL; denied HTTP(S) with zero server connections and working local resource; build-policy checks; concurrent synthetic games, lock refusal and retained-log checks |
| 17–22 | Streamed media/update/backups; identity-only link-removal journal; validator lifecycle and HTTP 304 refusal; optional API cross-check with mandatory pin; managed-key remerge; settings JSON, generated step IDs and media verification modes; HotD planning only | 108 installer cases, including loopback HTTPS, link update/removal/recovery, altered config, CLI settings and network-free HotD plan |
| 23, partial | Failed runs keep restored installed files/version and record failed, previous state and error | Failed-update and interrupted recovery cases; destructive recovery/pruning deferred below |
| 24–29 | Required MAME CHD header SHA; nonmerge anchors and verified candidate preference; BIOS alternatives/rows; multilingual nonretail index exclusion; byte-bounded CHD maps; route-specific and optional media; verified clone launch pin | 28 scanner and 35 catalog cases; clone membership/argument rejection; valid >1M-hunk sparse fixture plus map-budget/cycle guards; Python serial/schema cases |
| 30, 31 | Identification limits documented; junction roots/children excluded; cached title/manufacturer sort keys and recent re-sort | Synthetic Windows junction and PC name-only proof; offscreen recent-order update; non-PS2 auto-index remains limited below |
| 32, 33 | Persistent C++ sectioned row model with stable inner card delegates; initial window size applied once | Actual card object, focus, persistent index and contentY survive a role-only change; resize survives settings updates |
| 35, partial; 36, UI portion | Save persists per-game settings for next install/repair; separate scan start/cancel, general operation error labels, Explore art and overlay button targets | Saved unknown values/arrays reach a synthetic owned install; duplicate scan start does not cancel; local art, error label and >=44×56 overlay pill assertions. Immediate config reapply deferred below |

## Design choices and limits

- Steam entry removal and payload uninstall remain separate transactions. Payload
  stays installed until the Steam removal is approved and succeeds. If payload
  uninstall subsequently fails, the approved Steam removal remains applied and
  the error is reported; no cross-system rollback is claimed.
- Native Windows launch errors are implemented through a message box and a
  portable log. Tests suppress the message box with the offscreen platform and
  verify the log; visible message-box acceptance remains an operator check.
- CHD identity recognizes valid v5 headers only and does not audit payload integrity. Parent-dependent or older CHDs remain unverified; runtime acceptance is still open.
- Launch log retention bounds the number of completed session logs per game;
  it does not impose a size limit on a currently running child's output.
- Privacy checks apply to the committed branch tip and PR text. This pass does
  not rewrite main or the previously published lane histories.

## Deferred with reasons

| Item | Remaining work | Reason / current behavior |
|---|---|---|
| 23 | Repair when both ownership generations are corrupt | No trusted ownership reconstruction receipt exists. The operation stays blocked and existing files remain preserved. |
| 23 | Prune old `.previous-<run>` directories | A durable per-generation ownership and launch/age retention receipt is needed to distinguish owned files from edited/unowned files. Old generations remain retained. |
| 22 stretch | Fresh HotD release archive expansion-cap and `.bin` validation | A fresh real release download requires owner approval. The shipped recipe has a network-free planning test; no real download is claimed here. |
| 30 | Automatic non-PS2 product-ID indexes and GDI/CUE traversal | The shipped public automatic index covers PS2. Other platforms need explicit catalog serials or a supplied custom index; recognized headers alone do not map them automatically. GDI/CUE dependency traversal remains unsupported. |
| 31 | Strong PC media identity | Executable-name matching is only a name-level candidate, not product/publisher/hash verification. Those additional databases and checks remain future work. |
| 34 | Move remaining GUI-thread I/O to worker jobs | Tool planning, ownership/removal previews, Steam account/art/preview/apply, state restore and detail README reads remain synchronous. Conversion needs immutable job snapshots, cancellation/lifetime handling and synchronized art state to preserve the reviewed approval sequence. No responsiveness or timing acceptance is claimed. |
| 35 Save, partial | Immediately reapply only changed settings-backed config keys | Save persists settings and passes them to the next owned install/repair. The installer currently exposes whole install/update/repair operations; immediate Save needs a constrained config-only transaction API that cannot replay other steps or trigger generation processing. No immediate write is claimed. |

## Open no-GPU acceptance gates

1. S1–S3: compositor texture acceptance, headset laser/keyboard interaction,
   desktop plus overlay behavior, registration/removal and runtime quit.
2. Real flat launches of Scud Race, Time Crisis and Time Crisis II from the Hub
   and their approved Steam shortcuts, including focus/exit behavior.
3. The first approved Steam library entry on the real owner account, its art
   and launch behavior. Synthetic VDF tests are not that acceptance run.
4. Grid frame timings, GPU cost and hardware smoothness.

These gates remain open. Software tests and green CI do not establish them.

## Verification record

- Combined P0/P1: 10/10 CTest suites passed locally, offscreen/software only.
- Parent hardening: 5/5 targeted suites passed; the additional fake-runtime
  failure regression also passed.
- Final integrated Windows validation: all 13 GPU-free CTest suites passed. The installer suite was rerun after its obsolete synthetic media kind was aligned with the shipped disc schema. The S2 benchmark remained deliberately skipped.
- Late media-alias regressions failed before the file-identity guard and passed afterward; the affected privacy, installer and integration suites then passed (3/3). The Linux aggregate-initializer warning was corrected and its affected local suites passed (4/4).
- Windows/Linux CI receipts are recorded in the integration PR checks; those builds do not establish device or runtime acceptance.
- Independent read-only source review found no remaining P0/P1 blocker or critical regression in the new paths; it confirmed the explicitly deferred immediate Save transaction.
- Scoped scanner/catalog validation: 413 records, zero errors, two existing warnings.
- Immutable dependency pins were cross-checked against the official upstream release references; action pins were resolved from official repositories.
