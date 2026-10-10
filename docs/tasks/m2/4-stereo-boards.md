# Session 4: Stereo boards. Real depth, one arcade board family at a time

You own the per-board VR renderers: `setups/<board>-vr/`. You report to the lead session. Read [README.md](README.md) first: lane table, rules for every session, report format.

## Goal

Take one arcade board family, get its 3D scene drawn correctly once per eye, hand it to the lead's libacvr through the renderer interface, then move to the next board.

**Order:** Namco System 22 → Sega Model 3 → NAOMI / Dreamcast → Namco System 23.

## How to work without a headset

Until libacvr is ready, and for every automated check after that, your renderer outputs **side by side to a desktop window or to image files**: left eye and right eye from two camera positions you choose. That needs no OpenXR and no headset. The lead publishes the renderer interface in the first two days; build against it from the start so plugging into libacvr is a link step, not a rewrite.

Running any game needs the owner's own game files and the GPU: ask first, one game at a time. Automated tests use synthetic scenes only. No game geometry, texture or capture goes into the repository or a report.

## Board 1: Namco System 22 (Time Crisis first, then the racers)

- Base: [namco22-decompile](https://github.com/spacestate1/namco22-decompile) (MIT), built from upstream with guarded build-time patches kept in `setups/namco22-vr/`. Never vendor game data. Its host interface and projection are described in [architecture.md](../../architecture.md), [libacvr-contract.md](../../libacvr-contract.md) ("Namco22 integration plan") and the hub wiki article `true-3d-stereo-techniques`.
- The board projects before the renderer sees the geometry (screen x, y and depth, `sx = cx + X·zoom/Z`), so rebuild eye-space positions from the projected polygons and draw them per eye. DR-89's Time Crisis VR does this on the same base and is the reference for what works.
- **Aim:** cast the gun's ray into the rebuilt scene, project the hit through the game's own camera, and convert to the gun input the game reads ([controls.md](../../controls.md) §1.1). Per-game calibration constants are yours.
- **Order of games:** Time Crisis (gun), then Rave Racer and the other System 22 racers, each with its world scale, HUD layer and camera notes in a small per-game file.

## What "correct" means (every board)

From the hub wiki article `stereo-3d-for-arcade-games`:

1. Parallel cameras with off-axis frusta; never toe-in.
2. A world scale per game so eye separation matches the world; state how you derived it.
3. HUD, text and 2D layers on one flat plane at a fixed depth; the aim dot at the depth of what it points at.
4. Sky and backdrops at infinity.
5. Geometry re-drawn every headset frame from the newest head pose, whatever rate the game runs at.
6. Nothing missing when the head turns: widen the game's own field of view or cull margin where it can be done (hotd2-vr does this for its games), and say so where it cannot.
7. Anything that comes right up to the face pulled back.
8. The game's own light-gun flash and similar screen effects removed in VR.

## Deliver per board

- `setups/<board>-vr/`: build scripts, patches, the backend that implements the lead's renderer interface, per-game files (world scale, calibration, HUD rules, known issues).
- Tests that run in CI without game content: synthetic polygon lists through unprojection and per-eye projection (a known cube must come back as a cube), aim round-trips (a ray at a known point must produce the expected gun coordinates), HUD separation.
- A side-by-side capture tool for owner-run checks, writing only to `.local/`.
- A short page per board: what is rendered correctly, what is not, and what the next board should reuse.

## Next boards (when the lead says the current one is done)

- **Sega Model 3:** Supermodel already holds model-space geometry and builds the frustum from the game's viewport data, so draw twice with per-eye cameras instead of unprojecting. Supermodel is GPL; the backend lives in our GPL tree and links MIT libacvr.
- **NAOMI / Dreamcast:** Flycast receives screen x, y and 1/w. hotd2-vr (GPL) shows the method, including widening each game's field of view through per-game profiles. Start with the games it does not cover.
- **Namco System 23:** MAME appears to project on the host side from model-space data and matrices. Confirm by reading the driver before planning; if true, it is a re-render like Model 3.

## Done when (board 1)

- Time Crisis renders side by side with correct depth order and no missing geometry through a full attract loop and one stage, on the owner's PC.
- Shots land where the gun points across the whole screen, checked with the game's own calibration screen.
- The backend runs through libacvr in the headset (with the lead).
- The System 22 racers render side by side with per-game files, ready for lane 3's ghost controls.
