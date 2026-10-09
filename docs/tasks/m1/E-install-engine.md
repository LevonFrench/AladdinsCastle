# M1 lane E: install engine (M1 subset)

**Branch:** `m1/e-install`. **Depends on:** A, C. **Read:** [install-engine.md](../../install-engine.md) (normative), [game-packages.md](../../game-packages.md) §4, [frontend.md](../../frontend.md) §3-§4, [emulator-manifests.md](../../emulator-manifests.md), [legal.md](../../legal.md), `data/emulators/supermodel.toml`, `mame.toml`, `pcsx2.toml`.

## Build (in `hub/src/core/install/`)

- **State machine and persisted state** (`user/state/installs/<game>/<variant>.toml`), the **write-ahead journal**, the **ownership manifest** with content-addressed backups, and **engine-generated uninstall**: all per install-engine.md.
- **Step kinds for M1:** `github-release`, `download` (pinned SHA-256), `locate-package`, `require-media`, `copy-media`, `extract` (zip, 7z, tar via one archive backend; zip-slip protection, size caps), `copy`, `write-config` (ini / cfg key=value / toml / json; structured `map`/`bool`/`type`, managed keys only), `shortcut` (delegates to lane G; stub until G lands), plus the implicit **verify**. **Not in M1:** `adb-install`, `registry`, `run`.
- **Download safety:** HTTPS only, host allow-list per manifest/recipe, pinned tags, SHA-256 (record-on-first-download where a manifest says so), resumable, content-addressed cache, GitHub API with a 6 h cache and rate-limit handling.
- **Content guard:** refuse to place archives containing known ROM/BIOS names or extensions from the guard list; never fetch game content (legal.md).
- **Event stream** consumed by lane D's console; log file `user/logs/install/<timestamp>.log`.
- **Emulator installs** use the same engine with `data/emulators/*.toml` (`gate`: `download-from-upstream-only`, `consent-install` (show `consent` text first), `locate-only`).

## Tests

Fake HTTPS GitHub server, golden plans and transcripts, crash injection at every journal point (roll back or forward), zip-slip and size-limit cases, uninstall leaves the tree byte-identical to before.

## Also in this lane: automatic flat variants

Implement [game-packages.md](../../game-packages.md) §4.1: generate "Play in <Emulator>" variants from `[routes]` + `data/emulators/*.toml`, substituting media into the manifest's `[launch]` args. Verify the M1 manifests' launch args against each emulator's real CLI on the owner's PC and correct the manifests where needed (Supermodel `<zip>`, MAME `<set> -rompath <dir>`, PCSX2 `-batch -nogui -- <disc>`).

## Acceptance

On the owner's PC: Supermodel installs from its official release (pinned, hash recorded, portable) and uninstalls cleanly; the owner's existing MAME and PCSX2 are located and verified, not reinstalled or modified; automatic variants exist for every game with a working route + manifest. No owner paths or file names in committed tests or logs.
