# Architecture (draft 0)

## 1. Overview

```
 Desktop                                   Headset (Quest 3 via Link / Air Link / Virtual Desktop)
┌───────────────────────────┐   launch   ┌─────────────────────────────────────────────┐
│ HUB (desktop app)          │──────────▶│ VR SETUP for one game (owns the OpenXR      │
│ library + art, detect,     │            │ session while it runs)                      │
│ install recipes, update,   │◀──────────│                                             │
│ launch, Steam shortcuts,   │  on exit   │  A. our true-3D setup: game engine/emulator │
│ adb install to Quest       │            │     + libacvr (shared VR runtime)           │
└───────────────────────────┘            │  B. third-party VR port (TC VR, VC2VR, ...) │
                                          │  C. theatre setup: acvr-theatre + emulator  │
                                          └─────────────────────────────────────────────┘
```

No VR lobby. The Hub runs on the desktop. Each installed game is a **VR setup** that opens straight into the headset and closes back to the desktop (or to the SteamVR library). Three kinds of setup, in order of preference:

1. **A. Our true-3D setups.** A game engine or emulator (namco22-decompile, Supermodel, PCSX2...) built with **libacvr**, our shared VR runtime library. It draws the game's own 3D scene in stereo; libacvr provides everything that should feel the same in every game.
2. **B. Third-party VR setups.** Existing VR ports (DR-89 Time Crisis VR, VC2VR, PenguinScreen2), installed and configured by a recipe and otherwise unmodified. Day-one value.
3. **C. Theatre setups.** **acvr-theatre**, our small OpenXR app, captures a flat emulator window (Windows.Graphics.Capture) onto a screen in VR, with libacvr guns and ghost controls feeding an input bridge. This covers everything without a stereo route yet.

## 2. libacvr: the shared VR runtime

A C/C++ library (C ABI) that every AladdinsCastle setup links, so they all behave the same:

| Module | Provides |
|---|---|
| Session | OpenXR instance, session and swapchains (Vulkan, D3D11 or OpenGL binding), refresh-rate selection with fallback, recenter, standing-height calibration |
| Frame | Multiview stereo helpers; display-list replay at headset rate (§2.2) |
| Gun | Gun models, aim ray → hit → projection through the game camera ([controls.md](controls.md) §1.1), off-screen/reload, cover/ducking, laser, recoil haptics, two-gun play |
| Ghost controls | Wheel, shifters, pedals, handlebars, levers and buttons from the game's control set ([controls.md](controls.md) §2) |
| Comfort | Vignette, horizon lock, cut fades, cockpit anchor |
| Overlay | The shared pause menu ([frontend.md](frontend.md) §2) |
| Config | Reads the merged TOML for this game (game, controls, bindings, profile) |
| Outputs | Recoil, lamps and force feedback events → controller haptics |

A setup integrates by implementing a small **backend contract**:

```c
// called by libacvr
void  game_step(void);                        // advance one native frame (about 60 Hz)
void  game_draw_eye(const acvr_eye *eye);     // draw the last game frame for one eye
bool  game_camera(acvr_game_camera *out);     // focal, centre, view: for aim + world anchor
void  game_set_inputs(const acvr_inputs *in); // gun x/y/trigger/offscreen, axes, buttons
void  game_poll_outputs(acvr_outputs *out);   // recoil, lamps, FFB, score, state
```

This maps directly onto namco22-decompile's existing host API (`ss22_host.h`, `ss22_prepare` / `ss22_draw`), which is why it is the first target.

### 2.1 Process model

- Our setups (kind A) are one process: engine plus libacvr, the same as DR-89's Time Crisis VR. That's simpler and lower-latency than streaming eye textures between processes.
- Theatre (kind C) is two processes: the unmodified emulator, and acvr-theatre, linked by window capture and the input bridge.
- **OPEN:** where a closed-source game needs a hook DLL (VC2VR-style), use a shared-memory seqlock between the hook and the VR process. VC2VR does this so a crash on the VR side can't take the game down.

### 2.2 Display-list replay (decoupled rendering)

Arcade games update at about 57-60 Hz. The headset wants 90 or 120 Hz with the newest head pose. So:

- Run game logic at its native rate (DR-89 uses a deadline clock at 59.906 Hz).
- Keep the last frame's geometry and **re-render it once per headset frame** with fresh eye poses (namco22: `ss22_prepare` once, `ss22_draw` per eye).

### 2.3 World anchor and camera

- The setup reports the **game camera** for every game frame: view transform and projection. For System 22 that is the focal length (e.g. 772.5625 px for Time Crisis) and screen centre, per `geo_hw.c`.
- libacvr chooses a **world anchor** so that the game camera sits at the player's calibrated head position.
  - *Rail mode* (gun games): the anchor follows the game camera. Real head movement adds on top.
  - *Cockpit mode* (racing): the anchor follows the car; ghost controls are fixed to it.
  - *Comfort options* ([controls.md](controls.md) §2.3) adjust how the anchor moves.
- Scale is set per game: `world_units_per_metre`.

### 2.4 How setups produce stereo: three proven techniques

Studied from source, 2026-10-08:

| Technique | Used by | How | Quality |
|---|---|---|---|
| **Unproject reconstruction** | DR-89 Time Crisis VR, VC2VR | Take the game's projected polygons (screen x, y plus view-space depth z, plus that polygon's camera: focal, cx, cy). Rebuild view-space vertices with `x = (sx - cx)·z/f`, `y = (cy - sy)·z/f`. Render the rebuilt triangles per eye with OpenXR view and projection. | True 6DoF stereo; only what the game drew is visible. VC2VR re-runs the world pass with the view rotated to the 6 cube faces to fill in what's behind you. |
| **Source-level host** | DR-89 on namco22-decompile | The engine exposes a host API (`ss22_host.h`) and splits a frame into `ss22_prepare()` (once per simulation step) and `ss22_draw()` (any number of times). The host draws once per eye (or with multiview) from one simulation step. | Best. No hooking; upstream patched only through guarded build-time edits (`patch_upstream.py`). |
| **Clip-space disparity + camera RAM writes** | PenguinScreen2 (PCSX2) | Per-vertex horizontal offset in the GS vertex shader, from the PS2 per-vertex Q (1/w) value. Head rotation written into the game's camera variables in emulated RAM. | Cheap. Stereo depth only as good as Q is as a depth proxy. Head rotation needs a per-game RAM profile. Not real 6DoF parallax. |

**Generalisation for AladdinsCastle:** unproject reconstruction works with any emulator or port that can give us per-polygon screen coordinates, view-space depth and projection parameters. That's the generic route for Model 2/3, NAOMI-class and PC-port backends: hook the point where the emulated geometry engine outputs projected vertices. For PS2-class hardware with programmable vertex units, the PenguinScreen2 approach is the realistic first step.

Gun aim in both shipping gun ports matches [controls.md](controls.md) §1.1: cast the ray against the rebuilt triangles, then project the hit through *that triangle's* game camera. Time Crisis VR writes the result straight into the arcade gun register (`x = 68 + nx·626`, `y = 43 + ny·241`). VC2VR has to steer the PC game's joystick-velocity input in a closed loop, which lags. Direct register or port writes are the target.

### 2.5 Gun mapping

Covered in [controls.md](controls.md) §1.1. The setup gets an already-projected screen point and an off-screen flag through `game_set_inputs`.

### 2.6 HUD

Arcade HUDs are 2D sprites on top of the 3D frame. Setups should put the HUD on a separate layer if they can. libacvr shows that layer at a comfortable fixed depth, as a head-locked panel or a panel fixed to the cabinet, whichever the user chooses. When the backend can't separate it, the HUD stays merged into the frame at screen depth.

## 3. Backend plan

| Tier | Backend | Games | Route | Notes |
|---|---|---|---|---|
| A | **namco22-vr** (ours) | Time Crisis, Rave Racer, Ace Driver, Dirt Dash, Prop Cycle, Tokyo Wars, Cyber Sled, Cyber Commando | namco22-decompile (MIT) + libacvr host; stereo through the geo_hw.c projection | Same base as DR-89 Time Crisis VR. Renderer accuracy is still being worked on upstream. |
| A | **supermodel-vr** (ours) | Sega Model 3: Scud Race, Daytona USA 2, Sega Rally 2, Le Mans 24, The Lost World, ... | Fork of Supermodel (GPL) adding per-eye frustum in New3D (`CNew3D::CalcViewport`) + libacvr | Clean renderer seam. Model 3 gun list to confirm. |
| A | **pcsx2-vr** | PS2 GunCon 2 games: Time Crisis 2/3, Crisis Zone, Vampire Night, Virtua Cop Elite Edition, ...; PS2 racers | Build on PenguinScreen2's stereo approach (GPL); add libacvr gun aim | PenguinScreen2 is Linux-only today |
| B | **third-party setups** | Time Crisis (DR-89), Virtua Cop 2 PC (VC2VR), PenguinScreen2 titles | Recipe installs and launches the existing app | Day-one value |
| C | **acvr-theatre** | Everything else: Model 2 Emulator, MAME, TeknoParrot (Lindbergh, RingEdge), Flycast, Dolphin, RPCS3, DuckStation | Window capture + input bridge | |
| R | Research | Model 2 (MAME TGP / sm2-emu), Lindbergh (lindbergh-loader GL interposer), Daytona XBLA recomp, OutRun 2006 (Tweaks/decomp), Dolphin (free-look/stereo) | Find each one's stereo seam | See [landscape.md](landscape.md) |

## 4. Input bridge (theatre setups)

Ordered by preference:

1. **Emulator-native APIs.** No device needed and per-player by design: MAME Lua plugins, a DemulShooter-style memory feed (DemulShooter already supports many Model 2/3, Lindbergh, RingWide and TeknoParrot gun games), and IPC APIs where emulators have them.
2. **vJoy (signed Brunner fork 2.2.x)** for wheels, pedals and analog axes, with HidHide to hide the real devices from games. Not ViGEmBus, which was archived in 2023.
3. **Absolute mouse in the window** for single-gun theatre games.
4. **Custom VHF HID driver** only if two absolute guns are needed at device level. It's a kernel driver, so it needs attestation signing, so it comes last.

Outputs (recoil, lamps, FFB) come back through MAMEHooker-style output events or DemulShooter and become controller haptics.

## 5. Platforms

- **v1: Windows x64 Hub + PCVR setups.** Quest 3 through Quest Link (cable, reference), Air Link or Virtual Desktop (VDXR). Any OpenXR runtime should work, with SteamVR supported. Graphics API chosen per setup; libacvr supports whatever the runtimes accept (**OPEN**: confirm against Link, VDXR and SteamVR).
- **Quest standalone setups:** libacvr is plain C/C++ and builds for Android/OpenXR. DR-89 already runs namco22 on Quest, so System 22 games are the natural first standalone setups. The Hub installs them over USB with adb. Emulator setups stay PC-only.
- **Linux/SteamOS:** desirable (PenguinScreen2 and lindbergh-loader are Linux-native). Not a v1 target.

## 6. Repository layout (planned)

```
aladdinscastle/
  docs/                 design docs (this folder)
  hub/                  desktop Hub app (library, recipes, installer, launcher)
  libacvr/              shared VR runtime library (C ABI) + test setup ("stereo cube")
  setups/
    namco22-vr/         libacvr host on namco22-decompile (submodule + guarded patches)
    supermodel-vr/      fork notes/patches
    acvr-theatre/       OpenXR window-capture theatre app
  recipes/              install recipes (TOML)
  data/                 built-in config: games, controls, guns
  schemas/              JSON Schemas for each config kind
  tools/                CLI: validate, explain, hash-check, pack
```
