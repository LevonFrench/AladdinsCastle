# M1 scanner and local art

The scanner reads only configured media/tool roots. It does not search credentials,
account/licence directories, follow directory symlinks or Windows junctions, download game content or
launch games. Results and local paths stay in the portable user's directory. Public
tests contain synthetic headers and a tiny synthetic archive encoded in C++.

## Interfaces and UI integration

`hub/src/core/scan/Scan.h` exposes value types `ScanOptions`, `ScanResult`, `Binding`,
`ToolBinding` and `FileIdentity`. `Scanner::run` runs independently of GUI objects,
accepts an atomic cancellation flag and reports progress maps. `ScanController`
runs it through QtConcurrent, queues progress onto the GUI thread, and merges only
scan-owned media/tool fields into **current full** runtime snapshots before model
updates. Installation state, jobs, selection and play timestamps survive an active
scan. Cancelled scans never apply partial library states or replace the cache.

Connect UI `scanRequested(QStringList)` to `ScanController::scan`, and UI cancellation
to `cancel`. Connect controller `scanStarted`, `progress(QVariantMap)`, and
`scanFinished(bool cancelled)` to an adapter. Lane D takes a success flag, so call UI scanFinished(!cancelled); never connect those booleans directly. `resultsReady(ScanResult)`
provides binding receipts and complete runtime snapshots. Enable the filter's
`scanComplete` only after a completed result. Requirement IDs use the catalog's
shared `mediaRequirementId`: set, serial, explicit ID, then `<gameId>-media-<index>`.
The catalog regression prevents an unnamed disc or BIOS requirement disappearing.

CLI usage: `hubtool --data-root <catalog> scan --request <private JSON file>`;
use `locate` instead of `scan` for tool discovery only. Request fields are
`mediaRoots`, `toolRoots`, `artRoots` (arrays), `userRoot`, optional `mameXml`,
`supermodelXml`, `serialIndex`, and `maxDepth` (0–16). Point at a folder or explicitly
configured drive root; scan access remains read-only. JSON stdout includes paths
and archive entry names, so owner runs must redirect it into the gitignored
`<repo>/.local/` directory. Never paste private receipts
into a PR. Portable state is `user/cache/scan.toml`, `scan-bindings.json`, and
`user/scan-folders.json`. Cache identity is absolute path + size + mtime.

## Identification and proof limits

ZIP reads EOCD/ZIP64 and the central directory, capped at 64 MiB / one million
entries. It compares stored CRC32 and uncompressed size with XML requirements.
It neither reads nor decompresses members. A renamed archive is matched by its
metadata, never by filename. Split clone parents and recursively required MAME
devices are searched across all inspected archives; device requirements are
reported only after that inventory is complete. Receipts include supporting
archive paths and actual matched clone identity. Arcade receipts also retain the metadata-declared clone family in `setCandidates`; flat plans use the matched identity only when both it and the catalog set belong to that allowlist. Set IDs reject whitespace, paths and option-like values. Legacy differing-identity receipts require a fresh scan; unchanged or absent identity remains compatible with the declared catalog set. Game candidate scores count non-merge ROMs; a verified candidate outranks any partial candidate. Each MAME `bios=` group is an alternative, both on machines and required devices, rather than a requirement to own every BIOS revision. Required `<disk sha1>` rows must also match a valid CHDv5 header in the matched set folder beside the archive (`<ROM root>/<set>/*.chd`); ZIP metadata alone cannot verify a disk-based game. The matching CHD paths are retained as supporting bindings. A match verifies **declared header
identity**, not the integrity of compressed payloads; there is no full ROM audit.

7z uses the pinned official 7-Zip C SDK (`25.01`, commit
`5e96a8279489832924056b1fa82f29d5837c9469`), whose C reader is public domain.
Only `SzArEx_Open` and metadata accessors are called. The SDK may decode compressed
**archive headers**; game members are never extracted. Reads and single metadata
allocations are bounded at 64 MiB, with a total live budget of 128 MiB. Encrypted, unsupported or missing-CRC metadata
stays unverified. This is intentionally separate from the install lane's libarchive
reader, which consumes downloaded payload streams to validate/extract them and
does not expose the CRC metadata needed here.

Located MAME supplies `-listxml`, with timeout/cancellation and a cache keyed by
executable path/size/mtime. No game is launched. An explicit XML path supports
offline tests. Model 3 matching uses Supermodel `Config/Games.xml`, supplied
explicitly or beside a located executable. Clone region/offset entries override
the inherited parent entries; this supports regional program variants without
requiring firmware from a different emulator's definition.

Disc reads are bounded: ISO9660 PVD, root directory (up to 2 MiB), SYSTEM.CNF
(up to 64 KiB), CHD v5 header SHA1, Dreamcast IP.BIN, Saturn header, and GC/Wii IDs.
PS2 BIOS detection checks ROMDIR/ROMVER structures in .bin/.rom/.rom0, not filenames. A discovered same-basename .rom1 is an optional companion path; .mec/.nvm/.inf/.diff auxiliary files are not read. Disc headers
with no catalog identity remain unverified. CHDv5 sparse decoding uses pinned libchdr to read only ISO9660 PVD/root/SYSTEM.CNF hunks. Dimensions are checked before map allocation: hunk bytes at most 1 MiB, logical bytes at most 100 GiB, normalized map at most 32 MiB (12 bytes per compressed-map entry or 4 per uncompressed entry), with compressed-map bytes also capped at 32 MiB. Reads stop at 64 MiB compressed input and 8 MiB / 128 decoded hunks; parent-dependent discs remain unverified. Combined header SHA1 is retained separately and never treated as an uncompressed disc hash. An explicit chd_sha1 field can identify that distinct combined hash. Raw 2352-byte PS1 data tracks support 16/24-byte sector prefixes. Multi-track GDI/CUE traversal and generic BIOS platforms remain
unverified unless supported identifying metadata is supplied; no guessed filename
match becomes Ready.

The bundled serial index contains 100 PS2 title/serial mappings from the
[official PCSX2 GameIndex](https://github.com/PCSX2/pcsx2/blob/aa7ab4306e269075784c7ac3eb4b45e6e6c53445/bin/resources/GameIndex.yaml),
including Time Crisis II regional releases and recognized retail GunCon/reprint bundles. Demos/trials remain distinct. It contains metadata facts only.
`tools/build_serial_index.py` reproduces the index from that pinned YAML without
network calls or copying emulator settings. Exact catalog/alternate-title matching
normalizes punctuation and the numbered title spelling `II`/`2`; unknown serials
never match by ISO filename. The shipped automatic serial-to-game lookup is PS2-only. Dreamcast, Saturn, GC/Wii and other disc identities require an explicit catalog `serial`/ID or an owner-supplied `serialIndex`; recognizing their header does not identify a game in the shipped index. GDI/CUE traversal is still unsupported. PC media matching is explicitly **name-only**: a catalog `find`/`exe` basename plus PE/ELF structure, reported as `executable-name-only`. File version is informational; there is no PC payload hash, product-ID or publisher check. Tool location verifies a
PE/ELF executable structure; it does not claim a downloaded install is verified.

## Local art

`art::Resolver` uses immutable catalog values, mutex-protected binding/root
snapshots and QImageReader decoding on workers. `art::Provider` is a
`QQuickAsyncImageProvider`: `image://art/<gameId>/<kind>`.

The order is user art (`user/art/<id>/<kind>` and role aliases), folders beside
matched media keyed by verified set and media basename, configured local art
roots, RetroArch `thumbnails/<playlist>/Named_*` using normalized titles, PCSX2
`covers/<serial>` (also a configured covers directory directly), then generated title/manufacturer art using hub colours. Regional cover serials must come from the bundled public serial-to-game index; separators may be normalized, but titles/filenames are never guessed. Generated logo/wheel output is transparent.
Corrupt or oversize images fall through. Decode reads in place; thumbnail cache
keys include source path/size/mtime and requested size under `user/cache/art/`.
There is no network art fetch or account access. Every requested shape has a
generated QImage fallback. MAME artwork ZIP bezel decoding and local video
playback remain separate future media features.

## Verification

Windows MSVC 2022 / shared Qt 6.8.3 / CMake 3.31.10 / Ninja 1.13 builds with `/WX`.
CTest includes `hub-smoke`, `hub-catalog`, and `hub-scan-art`. Scanner/art tests cover
synthetic ZIP/7z metadata, ISO serials, CHD/Dreamcast/Saturn/GC/Wii headers, public
serial indexing without filename guesses, split clones, missing/external device
ROMs, cache invalidation, cancellation, scoped roots, art source priority/fallback
and asynchronous preservation of runtime fields. CI builds on Windows and Linux;
Linux execution and actual UI/owner-library art acceptance must be reported
separately from the local headless tests.

Primary backend reference:
[7-Zip C reader](https://github.com/ip7z/7zip/blob/5e96a8279489832924056b1fa82f29d5837c9469/C/7zArcIn.c),
[C API notes](https://github.com/ip7z/7zip/blob/5e96a8279489832924056b1fa82f29d5837c9469/DOC/7zC.txt).

Owner verification passed; receipts are kept privately in `.local/`.

Sparse CHD dependency: [libchdr](https://github.com/rtissera/libchdr/tree/607694ca0812edfc9cc2030c64634fc2393668de), BSD-3-Clause; bundled LZMA 26.02 public domain, miniz 3.1.2 MIT, zstd 1.5.7 BSD/GPL, dr_flac notices are preserved with build licenses. QFile callbacks retain Unicode paths and cancellation. The compressed 302-byte synthetic CHDv5 fixture is reproducible with tools/make_synthetic_chd_fixture.py and a user-supplied chdman; no owned bytes are used.

Private local art receipts can be produced with arttool --request <JSON-file>. Request fields: dataRoot, userRoot, artRoots, bindingsFile, gameIds, outputDirectory, optional kind. It uses the same resolver and writes CPU-rendered PNG previews plus receipt.json to the requested private folder. No visible window or game is launched.

Bindings also expose supportRequirements: set, sourcePath, entries (sourceName, targetName, crc32, size). Install/launch code can plan isolated aliases from verified required bytes if needed; scanner never writes game content or alters originals.

Provider URLs can append ?v=<scanController.artRevision>. On a successful scan, resultsReady updates resolver bindings/roots, then artRevision increments and artRevisionChanged fires. QML caches within that revision; the changed URL refreshes prior generated/local art. The provider strips the query before resolving its kind. Async image cancellation waits for bounded decode completion and always emits finished, without requesting a cancelled QFuture result.

ScanResult.elapsedMs measures completed run time; fileCount, archiveEntryCount and cacheHits are exported in private receipts. CRC-provider and set-family indexes are built once per run.

Post-review metadata guards: pinned libchdr remains unchanged. Before each chd_read, HeaderSafety resolves normalized v5 self-references with visited-hunk checks, target/map bounds and a 32-step limit. Only terminal codec 0-3 or direct uncompressed type 4 is decoded; parent and all other indirect types remain unverified. LOWRAM mode is disabled so the pinned API's normalized map is available. No recursive self-reference decoder path is entered. Synthetic normalized-map tests cover valid short references, cycles, target overflow, depth cap, indirect forms, cancellation and truncated maps.

The 7z adapter records live SDK allocation extents and validates member-count, name offsets/storage, directory bits, unpack positions and optional CRC storage before accessor calls. Missing optional Name properties remain unverified instead of dereferencing absent offsets. Tests include an ordinary synthetic empty-file 7z with Name omitted. Live metadata allocation count is capped at 4096. Cache schema 4 invalidates older reader results after the map-budget and junction/proof changes.

Synthetic Release verification passed. Owner verification passed; receipts are kept privately in `.local/`.

Owner verification passed; receipts are kept privately in `.local/`. Scan-time tool fingerprints and the synthetic changed-after-scan regression are covered by automated tests.

Private art exports accept optional width/height (1-4096 each, default 920x430), so Steam header/capsule/hero/logo previews use their actual intended aspect ratios. The subprocess fixture checks default export, custom portrait dimensions and oversized-request refusal.
