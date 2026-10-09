# M1 lane E: install engine (M1 subset)

**Branch:** `m1/e-install`. **Depends on:** A, C. **Read:** [install-engine.md](../../install-engine.md) (normative), [game-packages.md](../../game-packages.md) §4, [frontend.md](../../frontend.md) §3-§4, [emulator-manifests.md](../../emulator-manifests.md), [legal.md](../../legal.md), `games/timecris/install.toml`, `games/vcop2/install.toml`.

## Build (in `hub/src/core/install/`)

- **State machine and persisted state** (`user/state/installs/<game>/<variant>.toml`), the **write-ahead journal**, the **ownership manifest** with content-addressed backups, and **engine-generated uninstall**: all per install-engine.md.
- **Step kinds for M1:** `github-release`, `download` (pinned SHA-256), `locate-package`, `require-media`, `copy-media`, `extract` (zip, 7z, tar via one archive backend; zip-slip protection, size caps), `copy`, `write-config` (ini / cfg key=value / toml / json; structured `map`/`bool`/`type`, managed keys only), `shortcut` (delegates to lane G; stub until G lands), plus the implicit **verify**. **Not in M1:** `adb-install`, `registry`, `run`.
- **Download safety:** HTTPS only, host allow-list per manifest/recipe, pinned tags, SHA-256 (record-on-first-download where a manifest says so), resumable, content-addressed cache, GitHub API with a 6 h cache and rate-limit handling.
- **Content guard:** refuse to place archives containing known ROM/BIOS names or extensions from the guard list; never fetch game content (legal.md).
- **Event stream** consumed by lane D's console; log file `user/logs/install/<timestamp>.log`.
- **Emulator installs** use the same engine with `data/emulators/*.toml` (`gate`: `download-from-upstream-only`, `consent-install` (show `consent` text first), `locate-only`).

## Tests

Fake HTTPS GitHub server, golden plans and transcripts, crash injection at every journal point (roll back or forward), zip-slip and size-limit cases, uninstall leaves the tree byte-identical to before.

## Acceptance

On the owner's PC: VC2VR installs into the Virtua Cop 2 PC folder (in-place variant, backups kept) and uninstalls cleanly. DR-89 Time Crisis VR installs from a user-located ROM-free zip plus the owner's own `timecris` set, writes `quest-options.cfg` from settings, verifies and uninstalls cleanly. No owner paths or file names in tests or logs committed.
