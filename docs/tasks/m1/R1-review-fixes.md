# M1 R1: review fixes on the integration branch

Review of `codex/m1-integration` at `430e016` (four read-only reviewers, key findings re-checked by hand). CI is green and the core is solid: downloads are pinned and verified, archive inspection and containment are thorough, the write-ahead journal and fault-injection tests are strong, the Steam VDF writer keeps unowned bytes exactly and fails closed, and the AppId formula correction is right. The items below are what stands between this branch and M1 sign-off.

## Ground rules for this pass

- **No GPU is available.** Do code fixes and GPU-free tests only: `QT_QPA_PLATFORM=offscreen`, software OpenGL (`opengl32sw.dll` / `QT_OPENGL=software`) for QML tests, as CI already does. Do not run SteamVR, emulators, headset checks or GPU timings, and do not claim them. Leave those acceptance gates open and say so.
- Work on `codex/m1-integration` (or a branch from it). It holds wiring that is in none of the lane PRs.
- The M1 ground rules still apply ([README.md](README.md)): privacy, licences, no system changes without the owner, tests in CI, a report per PR.
- Do not merge anything. Do not rewrite history on `main`.
- Where a fix needs a design choice, pick the simplest option that matches the spec, write it in the PR, and carry on.

Line numbers refer to `430e016`.

## P0: privacy (before anything else)

1. **Owner scan receipts in a committed doc.** `docs/SCANNER-ART.md:112-120`, `:139`, `:141` describe the owner's library (file counts, which sets, discs, BIOS and art they hold). Replace with "Owner verification passed; receipts are kept privately in `.local/`." Same treatment in `docs/INSTALL-ENGINE-M1.md`, `docs/LAUNCH-STEAM-M1.md` and `docs/spikes/*.md` if they carry similar receipts.
2. **Absolute local path.** `docs/SCANNER-ART.md:32` uses the owner's drive path. Use `<repo>/.local/`. Grep every committed file for drive-letter paths and fix all of them.
3. **Test name mirrors an owner file.** `hub/tests/ScanArtTest.cpp:548` uses a real cover serial. Use an obviously synthetic serial.
4. **Owner hardware.** `docs/spikes/s2-grid.md:3` and `docs/spikes/s1-s3-overlay.md:65` name the owner's exact PC and headset. Use a hardware class ("desktop, recent high-end GPU") instead.
5. **PR text.** Edit PR #5's description the same way (it repeats the receipts).

## P1: safety and policy

6. **Licence gate lets installs through.** `hub/src/core/install/Install.cpp:1298-1305` blocks only the exact strings `locate-only` and `consent-install`. Our manifests use free-text gates (DuckStation, TeknoParrot, Dolphin, Cxbx-Reloaded, Model 2 Emulator), which the spec says block automatic install ([emulator-manifests.md](../../emulator-manifests.md) §2.1, §4). Switch to an allow-list: install only when `redistribution == "download-from-upstream-only"` and `gate` is absent, or `gate == "consent-install"` with consent given. Everything else is locate or search only, with the gate text shown to the user. Test every manifest in `data/emulators/`.
7. **Content guard.** `hub/src/core/install/Support.cpp:339-340` hard-codes extensions, never reads `extensions` from `data/content-guard.toml`, and fails open when the file is missing. Read the file, fail closed if it is missing, and add `.cdi`, `.gcm`, `.pbp`, `.rvz`, `.wbfs`.
8. **Media can land in install folders.** `Install.cpp:943-945` (extract `from` not checked at plan time) and `:745-755` (`copy.from` can resolve to a `require-media` output). Reject media paths and media step outputs as `extract` / `copy` sources (spec §6.7, §8.9).

## P1: M1 acceptance blockers

9. **Hardware filter tree empty, chips show "(undefined)".** `hub/src/models/FilterSortModel.cpp:137-174` `choices()` returns no `count`; `hub/src/ui/UiController.cpp:93` drops every board with count 0; `FiltersDrawer.qml:20,28` prints `modelData.count`. Add faceted counts. Add a UI test asserting three tree levels with counts above zero and no "undefined" text.
10. **Hub exe is a console program; Steam launch errors are invisible.** The built `aladdinscastle-hub.exe` has PE subsystem 3: a console window opens with the overlay and closing it kills the overlay. `hub/src/app/main.cpp:38-52` writes `--launch` failures (preflight and `start()`) only to stderr. Set `WIN32_EXECUTABLE` on `aladdinscastle-hub` (`hub/CMakeLists.txt:102`), use `AttachConsole(ATTACH_PARENT_PROCESS)` for CLI flags, and for `--launch` failures write `user/logs/launch-<id>-<time>.log` and show a native message box ([launch-and-runtime.md](../../launch-and-runtime.md) open decision 1). Test the log path headlessly.
11. **Retry reinstalls the wrong thing.** `HubServices.cpp:52-56,131` retries the session-wide last install (Supermodel, for example); `UiController.cpp:66` sends no ids. Pass game and variant ids, rebuild the request, and tell the user when there is nothing to retry.
12. **No way to stop a hung game.** `LaunchService::stop()` (`Launch.cpp:277`) has no UI caller, and `HubServices.cpp:62` silently ignores Play while busy. Add Stop (graceful, then force after a timeout) and show a message when Play is refused.
13. **Uninstall leaves the Steam entry.** `HubServices.cpp:58-60,76-111` never sets `steamRequest_.remove`. Offer the removal preview and approval in the uninstall flow (G brief item 5, spec §5.5).
14. **Steam update overwrites user changes.** `Steam.cpp:178-187` resets `icon`, `IsHidden`, `AllowOverlay`, `AllowDesktopConfig`, `LastPlayTime` on the owned entry while the preview shows only hashes (`:241-255`). Keep existing values of those fields on update.
15. **Steam shortcut does not pin the variant.** `Steam.cpp:177` writes `--launch <id>` only, so Steam may start a different variant from the one previewed. Add `--variant <id>` to the launch options and to `--launch` parsing.
16. **Steam write window and backups.** Re-hash `shortcuts.vdf` immediately before `atomicWrite` (`Steam.cpp:313/330`). Skip no-op writes (`:326`), and keep the first pre-Hub backup forever instead of pruning it (`:86-87`).

## P2: install engine correctness

17. **Update breaks hard-linked media.** `Install.cpp:842-866`, `356-361`: staging copies live files with `rawWrite(dest, readBytes(src))`, so an update of any `copy-media` variant hits `E_WRITE_CONFLICT` and reads whole discs into RAM. Skip `user-media-link` rows when staging, stream all copies, and add an update test with `copy-media`.
18. **Stale download validator.** `ArtifactStore.cpp:113-114,163,234,364-368,382`: `.part.validator` is never deleted, then a 304 is taken as success and a missing `.part` is hashed. Delete the validator on promotion and on mismatch, send validators only with `Range`, and treat 304 as an error for file downloads. Add a hash-mismatch test (a wrong pin fed to `acquire`).
19. **GitHub API is a hard dependency.** `ArtifactStore.cpp:284-287,340-355`: an uncached `github-release` fails when `api.github.com` is unavailable. The spec says the API is only a cross-check (§6.1, §8.4). Warn and rely on the pin.
20. **Config re-merge.** `Install.cpp:423-437` drops the merged result once the target file changed since the last write (Flycast rewrites `emu.cfg` on exit). Merge managed keys into the current bytes and keep per-key `prior` values (§6.8).
21. **Media link removal backs up whole files.** `Install.cpp:301-302`, called at `:1173`. Journal link removal without a byte backup; the original stays where it is.
22. **Recipe features the catalog already uses.**
    - Pass per-game settings into `Request.settings` from `HubServices::gameRequest` and `hubtool` (recipes use `from = "settings.*"`).
    - Honour `require-media` `verify = "none" | "name"` (`Install.cpp:916-921`).
    - Make step `id` optional unless another step refers to it (`Install.cpp:694`, spec §5.1), or generate ids.
    - Add a dry-run planning test (no network) for `games/dc-house-of-the-dead-2/install.toml` variant `hotd2-vr-pcvr` (D44). Downloading that release for real needs the owner's OK; if you get it, check the zip against the expansion cap and the `.bin` rule.
23. **State machine.** Both state generations corrupt blocks Repair (`Support.cpp:148-150,181`). Failed runs record no `previous_state` or `last_error`, and failed update/repair write back the old state (`Install.cpp:577-584,1203-1211`). `.previous-<run>` folders are never removed. Bring these in line with spec §3–4.

## P2: scanner and catalog

24. **MAME `<disk>` ignored.** `Scan.cpp:203-228` parses only `rom` / `file`, so a game whose data is on a CHD (NAOMI GD-ROM, Chihiro, System 246, Lindbergh) verifies from the zip alone. Parse `<disk sha1>` and require the matching CHD (header SHA-1, `.chd` in the set folder beside the zip).
25. **BIOS archive shadows the game.** `Scan.cpp:344,403-406`: merge (BIOS) ROMs count toward the score and an unverified file's score becomes the bar. Score only non-merge ROMs, evaluate every candidate, and prefer verified over higher score. Also honour `bios=` alternatives (`:211-227`).
26. **BIOS rows.** `kind = "bios"` rows that name a `set` must go through `matchSet` (`Scan.cpp:711-718`). A BIOS alone must not put a game in "In my library" (`GameRecord.cpp:154`).
27. **Demo discs become full games.** `tools/build_serial_index.py:20-23` strips non-ASCII, so trial, store-demo, replay and test discs map to the full game. Keep CJK in the key, deny-list 体験版, 試遊, リプレイ, 実験, and regenerate the index.
28. **Large PS2 CHDs rejected.** `Headers.cpp:366` caps hunks at 1,000,000 (about 3.8 GiB). Bound the map in bytes instead.
29. **Generated "Play in" variants need every media row.** `CatalogLoader.cpp:559-563`: vcop2's PC executable blocks Play in MAME, and an optional Dreamcast BIOS blocks Flycast forever. Pick media per route (arcade sets for arcade emulators, discs for console emulators, `pc-game` only for PC routes). Add `optional = true` on `[[media]]` (never blocks) and set it on the BIOS rows whose note says optional. Update [game-schema.md](../../game-schema.md) and the validator in the same change.
30. **Non-PS2 disc ids are never looked up** (`Headers.cpp:486-496`, `Scan.cpp:724-729`). Either add per-platform id indexes or state the limit plainly in `SCANNER-ART.md`.
31. Junctions are followed (`Scan.cpp:69,89`: also check `isJunction()`). PC media is matched by name only (`Scan.cpp:754-762`). Precompute a sort key instead of normalising per comparison (`FilterSortModel.cpp:286`), and make sure `lastPlayed` changes re-sort "recent".

## P3: UI, overlay, packaging

32. **Grid rebuilds and jumps to the top** on every model change (`UiController.cpp:20,95-97`, `GameGrid.qml:17,24`). Use a C++ row model with persistent indexes; a role change must not rebuild ([ui/05-models.md](../../ui/05-models.md) §7.5).
33. **Window size snaps back** (`DesktopShell.qml:7-8` binds size to settings). Set it once in `Component.onCompleted`.
34. **Blocking I/O on the GUI thread**: `HubServices.cpp:85-88,99,107,137`, `UiController.cpp:40`. Move to `QtConcurrent` with queued results (a frozen GUI also freezes the overlay tick).
35. Per-game settings Save is not connected (`UiController.cpp:22`, `DetailPage.qml:56`). The startup scan cancels itself in `--overlay --window` (`UiController::scan` toggles; split start and cancel). Errors that are not installs are shown as "INSTALL STOPPED" (`HubServices.cpp:113`). Explore tiles pass no art source (`ExplorePage.qml:21`).
36. **Overlay (code only; headset checks stay deferred).** Pass `vrOverlayMode` to every button so targets reach 44/56 px (04 §9.2). Overlay canvas 1280×800 per 04 §9.1 (`OverlayHost.cpp:23`), or record why 900. If `VR_Init` fails, fall back to the desktop window instead of exiting (`main.cpp:83`). Delay-load `openvr_api.dll` so the desktop Hub starts without it.
37. **README rendering.** Install a `QQmlNetworkAccessManagerFactory` that denies http(s), or render Markdown with `MarkdownNoHTML`; keep the regex as a second layer.
38. **Pins and CI.** Pin zlib, xz, libarchive and toml++ by commit SHA (`ArchiveDependencies.cmake:3,18,41`). Pin GitHub Actions by SHA (`ci.yml:42,49`). Add `$env:GITHUB_SHA` to `BUILD-INFO.txt` (`portable-windows.ps1:73`). Update the stale `docs/dev-setup.md:4,74`.
39. **Smaller items.** Lock `last-played.json` (`Launch.cpp:303-311`). Rotate or cap launch logs (`Launch.cpp:193-201`). Add VDF fixtures shaped like Steam's own files (a `tags` map, `FlatpakAppID`), plus tests for Steam starting mid-apply, more than one account, and `--launch` with spaces and Unicode in paths. Label the AppId formula in `launch-and-runtime.md` as Steam ROM Manager's convention, not Steam's own behaviour.

## Done when

- All P0 and P1 items are fixed with tests, and P2/P3 items are fixed or listed in the PR as deferred, with a reason.
- CI is green on Windows and Linux.
- `codex/m1-integration` is pushed and one PR to `main` is open ("M1: integrated Hub"). It lists what was fixed, verified and deferred, and names the gates still open because there is no GPU: S1–S3 overlay checks, real flat launches of the three games, the first approved Steam entry, and grid frame timings.
- PRs #1–#7 are marked as superseded by that PR, not closed or merged by you; the owner decides.
