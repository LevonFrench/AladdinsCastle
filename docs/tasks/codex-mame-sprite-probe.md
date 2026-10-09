# Task for Codex: MAME sprite-depth probe (OutRun first)

**Repo:** `J:\projects\games\aladdinscastle` (public, GPL-3.0). **Branch:** `codex/mame-sprite-probe` (use a git worktree, see the end). **Type:** experiment + small tool; evidence decides the design of sprite-scaler games in VR.

## Question to answer

Can we place every scaled sprite in a sprite-scaler arcade game at a believable depth in VR **automatically**, from what MAME exposes (sprite zoom, screen position, priority, road data), without game-specific world-Z data?

Hypothesis: these games compute sprite size ≈ k / distance, so `z ≈ k / zoom` per game (calibrated once), cross-checked by where a sprite touches the road (road scanline → ground distance). If both estimates agree, VR depth for scaler games can be mostly automatic plus a small per-game profile.

## Read first

- `J:\projects\games\aladdinscastle\docs\architecture.md` §2.4 (stereo techniques) and `docs\catalog.md`
- Wiki notes (local, read-only):
  - `J:\projects\.wiki\topics\vr-arcade-gun-racing\raw\articles\2026-10-08-scaler-mame-sega-namco-taito-sprite-sources.md` (sprite devices, OutRun sprite RAM `0x100000-0x100fff`, zoom fields `vzoom`/`hzoom` 0x200 = full, Y Board affine ground)
  - `J:\projects\.wiki\topics\vr-arcade-gun-racing\raw\repos\2026-10-08-gap2-input-mame-lua-outputs.md` (MAME Lua API notes)
  - `J:\projects\.wiki\topics\vr-arcade-gun-racing\raw\repos\2026-10-08-scaler-djyt-cannonball.md` (Cannonball reads true `oentry.z`: use as a **reference for expected values only**; its licence is non-commercial, so copy no code)
- MAME source (BSD-3 / GPL-2.0+): `src/mame/sega/segaorun.cpp`, `src/mame/sega/sega16sp.cpp` (`sega_outrun_sprite_device`, sprite format comment ~line 1059), `src/mame/sega/segaic16.cpp` (road), and the Lua engine docs `docs/source/luascript/` (MAME 0.289).

## Local setup (private: not in this file)

The owner's MAME 0.289 install path and ROM folder (which contains `outrun.zip`) are listed in `J:\projects\games\aladdinscastle\AGENCY.md` ("Owner's local library"). Read them from there. **Never write into those folders** except MAME's own `plugins` directory for the probe plugin, and remove it again when done (or ask the owner first if a plugin folder write is not acceptable; an `-autoboot_script` with `-plugin` disabled avoids touching MAME's folders entirely, so prefer that).

## Deliverables (only these paths, on your branch)

1. `tools/mame-probe/outrun_sprites.lua`: a MAME Lua autoboot script (`mame outrun -autoboot_script <path> -window -nothrottle` or similar) that, each frame:
   - reads the active sprite list (after the RAM swap, see `draw_write()` notes) and decodes per sprite: screen X/Y top/bottom, hzoom, vzoom, priority, hidden/end flags, bank/offset (enough to tell sprites apart);
   - reads the road layer state (road RAM / registers in `segaic16.cpp` for the OutRun road) per scanline or as much as is decodable;
   - writes JSON Lines to `.local/mame-probe/outrun-<timestamp>.jsonl` (gitignored), one line per frame;
   - stops after N frames (default 3000) and exits MAME cleanly.
   Use `manager.machine.devices[...].spaces["program"]:read_u16(...)` (or the 0.289 equivalent) and verify each API call against MAME 0.289 source/docs.
2. `tools/mame-probe/analyze.py` (Python 3.12, stdlib only): reads a JSONL capture and reports:
   - distribution of zoom values; for sprites whose bottom edge sits on the road, fit `z_road(screen_y)` vs `1/zoom` and print the correlation and fitted `k`;
   - outliers (HUD, background, horizon layers, giant painted sprites) that don't fit;
   - writes a small summary JSON and optional PNG-free text plots into `.local/`.
3. `docs/scaler-depth-probe.md`: the method, the exact MAME APIs used, the decoded sprite/road fields with MAME file:line refs, the results (numbers only, no ROM data, no screenshots of game art), and the verdict: is `z = k/zoom` (+ road cross-check) good enough for VR, and what needs per-game authoring.
4. If OutRun works and time allows: repeat for **Power Drift** or **Galaxy Force II** (Y Board: also capture the 6 affine ground parameters `currx, curry, dxx, dxy, dyx, dyy`) and **Chase H.Q.** (Taito Z: zoom + 2 priorities + road). The owner's library list in AGENCY.md says which of these sets exist; skip any that aren't present.

## Rules

- Read-only on the owner's ROM folders. No ROM content, sprite graphics or screenshots in commits. Captures stay in `.local/` (gitignored).
- Don't modify `games\`, `data\`, `docs\ui\`, other docs, or `tools\validate_catalog.py` (other work is ongoing).
- No code copied from Cannonball (non-commercial licence). MAME code may be referenced by file:line.
- Use full absolute paths (`J:\...`) when reporting.

## Worktree (so you don't share the main checkout)

```bash
git -C J:/projects/games/aladdinscastle worktree add J:/projects/games/aladdinscastle-codex-probe -b codex/mame-sprite-probe main
```

Work in `J:\projects\games\aladdinscastle-codex-probe`. The `.local/` folder there is gitignored too.

## Done when

The OutRun capture runs, `analyze.py` prints a fitted `k` and a correlation for road-contact sprites, and `docs/scaler-depth-probe.md` gives a clear verdict with numbers. Everything is committed on `codex/mame-sprite-probe` (not pushed). Summary lists open questions.
