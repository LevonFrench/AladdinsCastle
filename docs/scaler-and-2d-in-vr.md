# Sprite-scaler and 2D games in VR: what M2 did, and how we do it

Owner request (2026-10-08): bring sprite-scaler racers (OutRun, Super Hang-On, Power Drift, Chase H.Q., Pole Position...) and 2D or mixed light gun games (Point Blank, Operation Wolf, Lethal Enforcers...) into VR with real depth, the way M2's **Sega 3D Classics** did on the Nintendo 3DS. Then go further, because VR also has head tracking.

Sources: M2 / SEGA developer interviews (SEGA Blog, Impress Game Watch, Siliconera, Nintendo Life), MAME source, MiSTer core docs. The wiki has the full notes: topic `vr-arcade-gun-racing`, raw notes `2026-10-08-m2-3dclassics-*` and `2026-10-08-scaler-*`, and the series table.

## 1. What M2 actually did

### Sprite-scaler arcade games (3D Space Harrier, 3D Super Hang-On, 3D OutRun, 3D Galaxy Force II, 3D Power Drift, 3D After Burner II)

- **Not plain emulation.** M2 calls the core "similar to emulation" but "not strictly emulation". For 3D Space Harrier, the original 68000 game code isn't run at all: they replaced it so the 3D could work.
- **Depth came from the game's own 3D data, not from the sprite scale.** Space Harrier and Super Hang-On were "built in 3D from the start". The arcade games kept object positions in 3D and projected them to scaled sprites, so M2 used those positions. No zoom-to-depth formula was ever published. (Inference: the world positions are in the game's RAM. Cannonball's OutRun engine confirms OutRun keeps a per-object Z.)
- **Galaxy Force II (Y Board, three 68000s):** the Space Harrier method didn't carry over. M2 wrote a dedicated Y Board renderer and still runs the 68000 code. Its single background layer was given depth by hand.
- **Roads were hand-reworked where needed:** OutRun's forked-road sections were redone so they look right in 3D. No road-depth algorithm was published.
- **60 fps by interpolation (OutRun):** the arcade ran objects at 30 fps. M2 got the road to 60 fps first, then converted each object one at a time against the ROM, computing in-between frames from the known car speed.
- **Moving-cabinet mode:** the screen tilts with the original cabinet's motion (Super Hang-On, OutRun "exaggerated to convey the impression"), with gyro support.

### 2D games (GigaDrive: Mega Drive titles such as 3D Sonic, 3D Streets of Rage 2, 3D Shinobi III)

- M2 built an **extended virtual Mega Drive** ("GigaDrive"): the original emulator core plus extra background layers (one set per eye), a **Z value per background layer, per raster line and per sprite**, and 64 KB more VRAM.
- **All depth was hand-authored.** The games had no depth data, so M2 surveyed the art by hand (parallax clouds and ocean, pairs of trees). They patched the original code where objects had to sit in front or behind (Sonic's loops). Flat art stayed flat (Ecco's maze).
- **Version 2 (Shinobi III)** added vertical multi-scrolling and depth on a diagonal for a single graphic. Per-line depth was possible but "very processor-intensive".

### What the 3DS versions did not have

**Fixed-disparity stereo only:** no head tracking, so no motion parallax. The picture edges were lost, because no art exists beyond the original frame. Interviews say nothing about convergence or eye separation.

## 2. What VR adds, and what it costs

| VR difference | Consequence | Our answer |
|---|---|---|
| **6DoF head tracking** | Layers must hold up under head motion, not just look right from one viewpoint. Billboards show parallax correctly; huge painted sprites "cardboard" if you move a lot. | True 3D placement (§3); limit head travel to a comfortable box around the original camera; fade layers that break |
| **Wide field of view** | The original screen is a small window into the world, and nothing exists outside it (M2's lost edges, much bigger in VR) | Show the game through a **window frame**: a cockpit, cabinet surround or vignette. Extend the ground and road procedurally to the sides, and wrap the sky backdrop around you. |
| **Headset rate (90-120 Hz)** vs 30/60 Hz game | Judder | M2's trick: interpolate object positions from known speed. Plus our display-list replay (draw the last game frame per eye at headset rate). |
| **Motion sickness** | Fast forward motion | Cockpit anchor, vignette. Hang-On's horizon never moves vertically, which helps. |

## 3. Our method: a layered 3D scene rebuilt every frame

```
 L0  Sky / backdrop      tilemap layers      -> far cylinder/dome (near infinity), wraps beyond the original frame
 L1  Ground / road       road generator data -> real 3D road mesh (exact on Y Board: 6 affine params/frame)
 L2  Objects             sprite list         -> billboards at depth z, sorted by z then priority
 L3  Player vehicle      player sprite       -> fixed in front of the camera, or hidden and replaced by our own cockpit model
 L4  HUD                 sprites/text        -> head-locked or cabinet-fixed panel
```

### L1 road and ground

- **OutRun / Hang-On / Super Hang-On:** the road generator gives the road per scanline (centre offset, width, colour stripes). With a fixed horizon line `y_h` and camera height `h`, scanline `y` lies at ground distance `z(y) = h · f / (y − y_h)`. Building a mesh from consecutive scanlines gives the 3D road surface, including curves (lateral offset) and hills (horizon shifts on OutRun). Forked roads need per-game handling, as M2 also found.
- **Y Board (Power Drift, Galaxy Force II, Rail Chase):** the ground is one affine transform per frame (`currx, curry, dxx, dxy, dyx, dyy`), so the ground plane is exact.
- **Pole Position:** road drawn in 8-pixel chunks plus a road vertical-position register.
- **Taito Z (Chase H.Q.):** road layer plus two sprite priorities (under or over the road).

### L2 object depth, best source first

1. **Game RAM world Z (per-game profile).** This is what M2 effectively used: OutRun-class games keep 3D object positions and project them. A profile gives the RAM address and format of the object table. It's the same idea as PenguinScreen2's per-game camera-RAM profiles.
2. **Inverse scale: `z = k / zoom`**, with `k` calibrated once per game. Valid because these games compute sprite size as roughly a constant over distance. Hang-On and Space Harrier use a zoom-ROM lookup instead (invert the table); Pole Position switches between two sizes per billboard (a discrete depth ladder).
3. **Ground contact:** a sprite whose bottom edge touches the road gets `z = z_road(bottom_y)`. This cross-checks (2) and catches calibration drift.

The fusion rule: use (1) when a profile exists, otherwise (2) checked by (3). Anything that fits neither (HUD, large backgrounds, explosions) goes to the profile's "flat layer" list.

**Being tested now:** Codex task `docs/tasks/codex-mame-sprite-probe.md` captures OutRun's sprite list and road data through MAME Lua and measures how well `z = k / zoom` agrees with road contact.

### Gun games in this style

Aiming works exactly as for 3D games ([controls.md](controls.md) §1.1): cast the ray against the reconstructed scene (billboards + ground), project the hit back through the original camera, and feed the game its gun coordinates. Line of Fire, Rail Chase, Space Gun and Operation Thunderbolt get the scaler pipeline.

## 4. 2D and mixed gun games (Point Blank, Operation Wolf, Lethal Enforcers...)

- **Theatre** (flat screen, motion-controller aiming) works for all of them, now.
- **Depth layers** (GigaDrive-style) per game profile: each tilemap plane gets its own depth; sprites get depth from a **size ladder** (Operation Wolf and Lethal Enforcers draw near/far enemies at a few fixed sizes) or from screen Y (things lower on screen are nearer), plus hand-set overrides. The gun ray hits the layered scene.
- **Point Blank** is a set of mini-games with changing layouts, so a theatre with optional per-stage layer depth is the realistic target.
- **Precedent for hand-built per-game 2D layer depth:** 3dSen VR (NES).

## 5. Automatic vs per-game authoring

| Automatic from emulator/game data | Needs a per-game profile (authored once, shared as a file) |
|---|---|
| Sprite screen positions, zoom, priority | Object-table RAM address/format for true world Z |
| Road per scanline → road mesh; Y Board ground plane | `k` calibration for `z = k/zoom`; zoom-ROM inversion table (Hang-On / Space Harrier) |
| Ground-contact depth cross-check | Which tilemap layers are sky / far / near; horizon handling |
| Interpolation to headset rate from frame deltas | Flat-layer list (HUD, big painted sprites); forked-road fixes |
| Gun-ray hit testing | 2D games: depth per layer, sprite size ladders, overrides |

Profiles live in `games/<id>/setup/depth.toml` (format to be defined after the probe), in the same open-ended spirit as everything else.

## 6. Data routes and licences

| Route | Data | Licence | Use |
|---|---|---|---|
| **MAME Lua** (`-autoboot_script`) | Sprite RAM, road RAM, Y Board affine params, any RAM | MAME GPL-2.0+ / BSD-3 | **First route.** Licence-clean; covers Sega, Namco, Taito, Konami, Jaleco boards |
| Cannonball (OutRun engine port) | True `oentry.z`, world X, road arrays | **Non-commercial**; modified redistributions must include full source. Conflicts with GPL. | Reference for expected values only. A Cannonball-VR fork would have to stay under Cannonball's licence and could only link libacvr if libacvr is permissively licensed (see open question) |
| Pole Position 3D remake (ppengine) | Reverse-engineered logic in true 3D | No licence | Reference only |
| MiSTer cores (Space Harrier/Hang-On, Y Board) | Hardware format docs | GPL-3.0 | Format reference |
| 3DS ports via CitraVR (Quest) | M2's own stereo | User's own copies | Already a fixed-disparity stereo route today; compatibility unconfirmed |

## 7. Open questions

1. **libacvr licence.** To link into engines under any licence (namco22 MIT, Supermodel GPL, Cannonball non-commercial), should libacvr be **MIT**, with the Hub and the rest of the repo staying GPL-3.0?
2. Replace the player's car or bike sprite with our own 3D cockpit model (first-person driving), or keep the arcade third-person view?
3. How much head movement to allow before billboards visibly "cardboard": a tight box around the camera, or free?
