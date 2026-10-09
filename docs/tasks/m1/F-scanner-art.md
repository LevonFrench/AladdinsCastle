# M1 lane F: media scanner and local art resolver

**Branch:** `m1/f-scanner`. **Depends on:** C. **Read:** [art-pipeline.md](../../art-pipeline.md) §0 (local art only), [game-schema.md](../../game-schema.md) `[[media]]`, [frontend.md](../../frontend.md) §4 (point / search), the Codex media-identification prototype and design if merged (`docs/media-identification.md`, `tools/identify/`), [hub-architecture.md](../../hub-architecture.md) §4.

## Build (in `hub/src/core/scan/` and `hub/src/core/art/`)

1. **Media scanner** (worker thread, progress, cancel):
   - MAME-style romsets (`.zip`/`.7z`): read the zip central directory CRC32s (no decompression) and match against MAME's `-listxml` (from the located MAME, cached in `user/cache/`), including renamed or old-name sets, parent/clone, and missing device sets (e.g. `namcoc74`).
   - Discs: PS2/PS1 serial from `SYSTEM.CNF` (ISO9660), CHD v5 header hashes, Dreamcast IP.BIN, Saturn header, GC/Wii game ID. Header reads only; no full hashing of large discs in M1.
   - PC games: `find` exe names + file version.
   - Output: per game, which `[[media]]` entries are satisfied, missing or wrong → feeds lane C's state resolution.
   - Cache by path + size + mtime in `user/cache/scan.toml`.
2. **Folder setup:** "Find my files" = point to folders or search drives (read-only). Saved in `user/` config.
3. **Local art resolver** (art-pipeline §0): `user/art/<id>/` → per-system folders beside the ROMs (`marquee`, `boxart`, `snap`, `wheel`, `fanart`, `images`, `media`), matched by ROM file basename / MAME set → RetroArch `thumbnails/<playlist>/Named_*` by normalised title → PCSX2 `covers/<serial>` → generated fallback (title + manufacturer colours from `[hub]`). Read in place; thumbnails cached under `user/cache/art/`. Exposed via the `image://art/<id>/<kind>` provider.

## Privacy

Test against the owner's folders from AGENCY.md **locally only**; reports go to `.local/`. Unit tests use synthetic zips and synthetic disc headers.

## Acceptance

On the owner's PC: the scan finds Time Crisis (including old-name sets mapped to current MAME names), the Model 3 sets and the PS2 GunCon discs by serial, flags missing device sets, and the library shows the owner's local art for those games and generated fallback elsewhere. Runtime stated in the PR.
