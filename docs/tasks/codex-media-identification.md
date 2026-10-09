# Task for Codex: media identification ("Find my files")

**Repo:** `J:\projects\games\aladdinscastle` (public, GPL-3.0). **Phase:** scoping, with a small read-only prototype. **Branch:** create `codex/media-identification` and commit there; do not push to `main`.

## Goal

Design how the AladdinsCastle Hub recognises the user's own game files **by content, not file name**, and prove it on the owner's real library. Example of why: a Rave Racer set saved under an old MAME name must still be recognised as `raverace` (World RV2 Ver.B), and the Hub must say "you're missing namcoc74" rather than "file not found".

## Read first

- `J:\projects\games\aladdinscastle\docs\game-schema.md`: `[[media]]` entries (kind `mame-romset`, `disc`, `pc-game`, `bios`; `set`, `serial`, `find`)
- `J:\projects\games\aladdinscastle\docs\frontend.md` §4 (install / locate / search for emulators and game files)
- `J:\projects\games\aladdinscastle\games\*\game.toml`: real examples (`timecris`, `raverace`, `scud`, `vcop2`, `ps2-virtua-cop-elite-edition`); more are being added by other agents right now
- `J:\projects\games\aladdinscastle\_refs\namco22-decompile\docs\ROM_CHECKSUMS.md`: per-chip checksums and accepted older chip names

## What to identify

| Kind | Method to design and test |
|---|---|
| MAME / Supermodel / TeknoParrot-era romsets (`.zip`, `.7z`) | Read the zip's central directory (CRC32 per member: no decompression needed). Match against MAME's own data: use the user's MAME install; `mame -listxml <set>` (or the full `-listxml`) gives every set's ROM names, sizes, CRC32 and SHA1, and parent/clone and merged/split/non-merged relationships. Report: matched set, completeness (missing/bad chips), required device/BIOS sets (e.g. `namcoc74`), clones vs parent, renamed sets. Supermodel: `_refs\Supermodel\Config\Games.xml` has its own ROM lists. |
| CHD (`.chd`) | Read the CHD v5 header only (raw SHA1 and data SHA1 are in the header). Design how to map to a game: a Redump/MAME hash list the user supplies or a lookup the Hub ships as data. Don't decompress. |
| PS2 / PS1 discs (`.iso`, `.bin/.cue`, `.chd`, `.cso`, `.gz`) | ISO9660: read `SYSTEM.CNF` → serial (e.g. `SLUS-20219`). For CHD/CSO, design the approach (chdman `info`/`extractcd` only if already on the PC; or libchdr later). |
| Dreamcast (`.gdi`, `.cdi`, `.chd`) | IP.BIN header (product number, title) |
| Saturn | System ID header (product number) |
| GameCube / Wii (`.iso`, `.gcz`, `.rvz`, `.wbfs`) | Disc header game ID (first 6 bytes); note which containers need decompression |
| PC games | `find` exe names plus file version info / size / hash |

## Deliverables (only these paths)

1. `J:\projects\games\aladdinscastle\docs\media-identification.md`: the design: per media kind, the method, the data source (MAME listxml, Supermodel Games.xml, Redump DATs, our own `data/hashes/`), how results map to `game.toml` `[[media]]`, how missing parts are reported in the UI ("Needs your files: namcoc74"), performance (header-only reads, caching by path+size+mtime), and privacy (results stay on the user's PC).
2. `J:\projects\games\aladdinscastle\tools\identify\`: a **Python 3.12, standard-library-only**, read-only prototype CLI:
   - `python tools/identify/identify.py <folder> [--mame <path to mame.exe>] [--games games] --out .local/identify-report.json`
   - Romsets matched via zip CRC32s against MAME `-listxml` output (cache the XML under `.local/`); discs matched via serial/header where cheap; prints a short human summary and writes the JSON report.
3. Add `.local/` to `J:\projects\games\aladdinscastle\.gitignore`.

## Test it on the owner's library (read-only)

- The owner's ROM, disc and PC-game folders and their MAME install path are listed in the local, gitignored project brief (`AGENCY.md`, section "Owner's local library"). Use those; don't copy them into any committed file.
- **Never write, move, rename, extract or delete anything in the owner's game folders.** Read headers and zip directories only; don't hash whole multi-GB discs in the prototype (note where full hashing would be needed).
- **Never commit the report or the owner's file names.** Reports go to `.local/` (gitignored). In the design doc, describe results generically ("an old-name Rave Racer set was correctly matched to `raverace`").

## Rules

- Don't modify `games\`, `docs\ui\`, `data\vocab\`, `_refs\`, `tools\validate_catalog.py` or other existing docs (other agents are writing there). Put suggested schema changes in your design doc.
- No downloading of game content or links to it. Downloading public hash DATs is out of scope for the prototype: design for it instead.
- Use full absolute paths (`J:\...`) when reporting.

## Done when

The design doc covers every media kind above; the prototype correctly identifies the owner's MAME and Model 3 sets (including an old-name set and a missing device set) and reads PS2 serials from at least the uncompressed `.iso` files; everything is committed on `codex/media-identification`; the summary lists what worked, what didn't, and open questions.
