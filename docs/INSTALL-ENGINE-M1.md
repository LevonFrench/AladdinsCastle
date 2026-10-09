# M1 install engine

The C++ engine in `hub/src/core/install/` implements the lane E subset of
`docs/install-engine.md`. It is shared by `hubtool` and the queued QObject
`InstallService`; GUI workers send typed events back to the GUI thread.

## Commands and adapter

`hubtool --data-root <catalog> --install-root <portable-root> plan-tool supermodel`
prints the same plan used by `install-tool supermodel`. `uninstall-tool supermodel`
uses only its recorded ownership rows. `locate-tool <id> <exe>` verifies filename,
Windows x64 PE structure and SHA-256 without executing or modifying that tool.
`plan`, `install`, `repair`, `update`, `recover`, `uninstall-preview`, and `uninstall`
take `<game> <variant>`. Optional `--recipe`, `--bindings` and `--handover` inputs
are local data. Bindings JSON has `media` and `tools` objects keyed by requirement
ID; media records contain `path`, `sha256` and `verified`.

The Qt event contains `ts`, `run`, `kind`, `step`, `index`, `total`, `text` and
`code`. The service emits `finished(bool, QString)` and `runtimeStateReady`.
Call `setRuntimeStateSource` with the repository's current snapshot provider so
scan, art and recent-play changes made during a worker run are preserved.

## Persistence and safety

State and manifests are TOML `schema=1` envelopes around `[data]`. Checksums use
sorted UTF-8 canonical JSON, hexadecimal control escapes and no floating-point
state values. Writes preserve `.previous`; a newer schema blocks writes even
when an older generation exists. Backups use SHA-256 names under `user/state`.
Every installed file and empty-directory mutation has a flushed intent before
the mutation, then a flushed done record. Manifests checkpoint after file writes
and steps. Commit order is manifest, journal commit, then installed state.
Recovery rolls back incomplete runs and rolls forward journalled commits.

One root-wide file lock serializes mutation across processes; the Qt service
queues additional requests. Uninstall compares bytes or managed config keys,
keeps edits and unowned files, restores prior bytes, and removes only empty
directories created by the engine. It never recursively deletes user folders.
Updates stage a complete generation, copy unowned files into it, verify it,
perform journalled renames, then verify and commit the live tree. Retained
previous generations are deliberately kept until explicit cleanup policy is
integrated; no startup cleanup deletes user data.

Archive ingestion uses project-local pinned libarchive 3.8.9, zlib 1.3.1 and
xz/liblzma 5.8.1, with upstream licence texts copied into build notices. ZIP,
7z, TAR and gzip pass one inspection layer before staging extraction. Paths,
links, case collisions, encryption, sizes, entry counts, CRC/truncation, known
content names/extensions and configured SHA-256 content hashes are checked.
`.bin` and `.img` are conservatively rejected for upstream payloads. Confirmed
user media uses a separate explicit local-copy/alias path.

HTTPS peers remain verified. Every redirect is allowlisted. Exact tags/assets
and recipe SHA-256 pins are required; GitHub metadata digests cross-check the
pin. Cache hits permit offline repair. Partial downloads use Range/If-Range;
host backoff is persisted. Record-on-first-download is an explicit tool-only
`record_sha256=true` manifest policy; it is not an implicit fallback for missing
hashes. The shipped Supermodel Windows artifact has a concrete reviewed pin.

## Flat launch preparation

`makeFlatLaunchPlan` resolves a generated variant into executable, argument
array, working directory and writable directories without launching anything.
`prepareFlatLaunch` applies only its explicit resource and fresh-profile plan
under `user/emulator-profiles`; it never reads existing profile INIs.
Preparation checkpoints pending file ownership before publishing and records
completion after durable writes. Resource retries retain receipts. An edited
Hub routing marker or unexpected portable.ini fails closed.
MAME uses explicit isolated output directories and all confirmed support folders.
Optional local device aliases repackage only scanner-confirmed entries, validating
the actual decompressed size and CRC before publishing. Their creation is off
by default when a read-only MAME audit already succeeds.

PCSX2's pinned v2.8.2 source appends `PCSX2` to `-datapath`, and existing portable
markers override it. The plan uses a fresh INI with `UI.SettingsVersion=1`,
`UI.SetupWizardIncomplete=false`, and the explicitly confirmed BIOS binding.
The located v2.7.24 source has no `-datapath` switch. To support both versions,
the plan always copies only executable, DLLs and public
resources into an isolated application directory, passes `-portable`, and
creates a Hub-owned `portable.txt` directing writes to the fresh profile.
It does not copy existing
INI, account, authentication or credential data. Supermodel uses `-window`, a
read-only Games.xml path and isolated Config/NVRAM/Saves/log paths.

## Scope and remaining acceptance

The nine M1 step kinds are implemented. `run`, `adb-install`, `registry`,
`patch-text` and in-place VR recipes fail during planning. The shortcut callback
is a lane G integration point; without it the engine emits a warning and commits
`installed-with-warnings`, never claims a shortcut was created. Steam writes and
emulator launches are outside this lane's automatic tests.

Synthetic tests cover all nine steps, canonical envelope vectors and fallback,
managed keys and config undo, archive formats and hostile names/links/caps,
fake HTTPS with a test-only localhost CA, event/plan transcripts, ownership,
portable profile isolation, and faults at install/update/uninstall journal,
manifest and state boundaries. Tests use zero survival wait; production uses
the specified three seconds. Windows MSVC is the local build gate; Linux CI,
actual owner emulator CLI behavior, FAT copy fallback, locked files, live GUI,
Steam and headset acceptance remain distinct gates.

Synthetic install verification passed. Owner verification passed; receipts are kept privately in `.local/`. Real emulator launches remain a separate acceptance gate.
