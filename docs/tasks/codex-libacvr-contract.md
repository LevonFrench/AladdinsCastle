# Task for Codex: draft the libacvr backend contract

**Repo:** `J:\projects\games\aladdinscastle` (public, GPL-3.0). **Phase:** scoping/design. **Branch:** create `codex/libacvr-contract` and commit there; do not push to `main`.

## Goal

Design the C ABI that every AladdinsCastle VR setup implements, so one shared runtime (**libacvr**) can drive any game engine or emulator in true-3D stereo with motion-controller guns and grabbable racing "ghost controls". Ground every decision in the two real codebases below, and show exactly how each one would implement the contract.

## Read first

- `J:\projects\games\aladdinscastle\docs\architecture.md`: §2 (libacvr modules, draft backend contract, display-list replay, world anchor, stereo techniques, gun mapping, HUD)
- `J:\projects\games\aladdinscastle\docs\controls.md`: gun aim math (§1.1), cover/ducking, ghost control types (§2.1) and grab rules
- `J:\projects\games\aladdinscastle\docs\config-spec.md` §5 and `J:\projects\games\aladdinscastle\games\raverace\setup\controls.toml`: control set format
- `J:\projects\games\aladdinscastle\docs\emulators.md`: why these two targets come first

## Code to study (local clones, read-only, gitignored)

1. **namco22-decompile**, `J:\projects\games\aladdinscastle\_refs\namco22-decompile\` (MIT)
   - `engine/ss22_host.h` (host callbacks + lifecycle symbols), `engine/ss22_input.h`, `engine/ss22_out.h`
   - `engine/ss22_gl.h` / `ss22_gl.c` (`ss22_prepare` once per sim step, `ss22_draw` any number of times)
   - `engine/geo_hw.h` / `geo_hw.c` (projection: `sx = cx + X*(mant*2^-shift)/Z`; `geo_view`; per-quad records)
   - `engine/ss22_run.c` (frame loop, 59.9 Hz), `engine/ss22_board.c` (gun port: x = 68..694, y = 43..284, off-screen)
   - `timecris/src/tc_game.c`, `raverace/` (cabinet input tables: gun, wheel, shifter, pedals)
2. **DR-89 Time Crisis VR**, `J:\projects\games\aladdinscastle\_refs\time-crisis-vr\` (MIT): the existing VR host on that engine
   - `quest/quest_host.c` (OpenXR loop, aim, cover, refresh rate), `quest/quest_scene.c` (unproject, `qvr_aim`), `quest/quest_gl.c` (multiview), `quest/quest_cover.c`, `quest/quest_clock.h`, `tools/patch_upstream.py` (guarded upstream patches), `docs/AIM-PROJECTION.md`, `docs/DEVELOPMENT.md`
   - Note: `upstream/` (submodule) is empty in this shallow clone; use `_refs\namco22-decompile` for the engine.
3. **Supermodel**, `J:\projects\games\aladdinscastle\_refs\Supermodel\` (GPL): Sega Model 3
   - `Src/Graphics/New3D/` (`New3D.cpp`: `CNew3D::CalcViewport`, `InitMatrixStack`; `R3DShader.cpp` projection upload), the frame loop and input system (`Src/Inputs/`: light gun axes, off-screen, analog wheel/pedals; `Config/Games.xml` per-game inputs for `scud`, `lostwsga`, `daytona2`, `srally2`)
4. For comparison only: **VC2VR** `J:\projects\games\aladdinscastle\_refs\Rea-Virtua-Cop-2-VR\` (`src/vc2_share.h` seqlock shared memory, `src/vc2vr.c` `aimFromPose`).

## Deliverables (only these paths)

1. `J:\projects\games\aladdinscastle\libacvr\include\acvr.h`: the draft public header.
   - Opaque handles, versioned structs (`size` + `version` fields), C99, no C++ in the ABI.
   - Backend → libacvr: `game_step`, `game_draw_eye` (or a multiview variant), `game_camera` (per-frame camera: focal, centre, view; per-polygon cameras if the game uses several, as in Time Crisis), `game_set_inputs`, `game_poll_outputs`, plus lifecycle (`open`, `close`, `pause`), save/score hooks if useful.
   - libacvr → backend: input structs for N guns (normalised screen x/y in the game's projection, trigger, off-screen, reload), analog axes (steering, accel, brake, lean, pedal speed), digital buttons (gear low/high, H-gate gears, sequential up/down, view, start, coin), cover/pedal.
   - Outputs: recoil/solenoid, lamps, force feedback (axis + strength), score/state events.
   - Graphics: how the backend gets eye render targets (GL / Vulkan / D3D11 handles), the timing contract (native sim rate vs headset rate: display-list replay), HUD layer separation.
2. `J:\projects\games\aladdinscastle\docs\libacvr-contract.md`: the design rationale.
   - For each struct/function: why, and which real code it maps to (file:line in the refs).
   - **namco22 mapping:** how a namco22 host implements the contract (which `ss22_*` symbols, where the guarded patches go, gun-port mapping, cover pedal bit, Rave Racer wheel/shifter/pedal mapping).
   - **Supermodel mapping:** where per-eye frustum and view offsets go in New3D, how the emulator loop would call `game_step` / `game_draw_eye`, how gun axes and analog inputs would be fed without RawInput, what's unknown.
   - Gaps and risks; open questions for the owner.
3. Optional: `J:\projects\games\aladdinscastle\libacvr\examples\stereo_cube_backend.c`: a minimal fake backend implementing the header (no OpenXR needed; compiles with `cc -std=c99 -c`).

## Rules

- **Don't modify** anything under `games\`, `docs\ui\`, `data\`, `_refs\`, `tools\`, or other existing docs (other agents are writing there). Suggest changes to `architecture.md` inside your rationale doc instead.
- No game content: no ROMs, BIOS files or links to them.
- Licences: our code is GPL-3.0. namco22 and DR-89 are MIT; Supermodel is GPL. Don't copy code; reference it by file:line.
- Use full absolute paths (`J:\...`) when reporting files.
- Check the header compiles: `gcc -std=c99 -fsyntax-only libacvr/include/acvr.h` (or `clang`), and the optional example with `-c`.

## Done when

`acvr.h` compiles; `libacvr-contract.md` explains every type with real file:line evidence and gives a step-by-step integration plan for namco22 and Supermodel; work is committed on branch `codex/libacvr-contract` with a short summary of open questions.
