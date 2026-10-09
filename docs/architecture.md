# Architecture (draft 0)

## 1. Overview

```
                    ┌──────────────────────────── Quest 3 (Link / Air Link / Virtual Desktop)
                    │
┌───────────────────┴──────────────────────┐
│  HALL  (Godot 4, owns the OpenXR session) │
│  - hall scene, cabinets, UI, list mode    │
│  - ghost controls, gun models, hands      │
│  - compositor: game eyes + depth + our 3D │
│  - config loader (layered TOML)           │
│  - backend manager                        │
└──────┬───────────────┬─────────────┬─────┘
       │ ACBP          │ hand-off    │ theatre
       ▼               ▼             ▼
┌─────────────┐ ┌─────────────┐ ┌──────────────────┐
│ ACBP backend│ │ External VR │ │ Flat emulator    │
│ (separate   │ │ app (owns   │ │ window, captured │
│  process)   │ │ the headset │ │ + input bridge   │
│ stereo eyes │ │ until exit) │ │ (MAME, M2Emu,    │
│ + depth +   │ │ TC VR, VC2VR│ │  TeknoParrot...) │
│ game camera │ │ PenguinScr2 │ │                  │
└─────────────┘ └─────────────┘ └──────────────────┘
```

There are three backend modes, in order of preference for a game:

1. **ACBP (AladdinsCastle Backend Protocol).** The backend renders the game's own 3D scene for the eye poses we send. It returns colour and depth per eye, plus the game camera it used. The hall composites the result and adds hands, gun and ghost controls with correct occlusion. Input goes straight to the game through the protocol, so no virtual devices are needed. This is the target for everything we build.
2. **Hand-off.** For existing VR ports (DR-89 Time Crisis VR, VC2VR, PenguinScreen2), the hall releases the headset, launches the app, and takes over again when it exits. No integration needed, so day-one value.
3. **Theatre.** A flat emulator window is captured (Windows.Graphics.Capture) and shown on the cabinet or a big screen. The controller ray is projected onto the screen plane and sent through an input bridge. This mode covers everything else.

## 2. ACBP backend protocol

### 2.1 Channels

| Channel | Transport | Content |
|---|---|---|
| Control | Named pipe or localhost socket, JSON-RPC | `hello` (capabilities, version), `load(game)`, `start`, `pause`, `resume`, `stop`, `state`, `config`, events (score, game over, lamps, recoil, FFB) |
| Pose & input | Shared-memory ring buffer, lock-free | Per headset frame: predicted display time, per-eye pose + FOV, controller poses, abstract game inputs (`gun[n].x/y/trigger/offscreen`, `axis.steering`, `button.*`) |
| Frames | Shared GPU textures (D3D11/12 shared NT handles or Vulkan external memory) + fence | Per eye colour + depth, plus metadata: frame id, pose id used, game camera view/projection, HUD layer (optional), near/far |

### 2.2 Display-list replay (decoupled rendering)

Arcade games update at about 57-60 Hz. The headset wants 90 or 120 Hz with the newest head pose. ACBP backends should:

- Run game logic at its native rate (as DR-89's port keeps "arcade timing independent of compositor half-rate").
- Keep the last frame's geometry, its transformed display list, and **re-render it once per headset frame** with fresh eye poses.

Head motion therefore stays smooth at the headset's rate even though the world updates at 60 Hz. For emulators, this means capturing the emulated GPU's draw list (as VC2VR does from the PC renderer, and PenguinScreen2 does from the PS2 GS/VU1 path) and replaying it.

### 2.3 World anchor and camera

- The backend reports the **game camera** for every game frame: the view transform and projection. For System 22 that is the focal length ("zoom", e.g. 772.5625 px for Time Crisis) and screen centre, per `geo_hw.c`.
- The hall chooses a **world anchor** so that the game camera sits at the player's calibrated head position.
  - *Rail mode* (gun games): the anchor follows the game camera every frame. You ride the camera, and your real head movement adds on top.
  - *Cockpit mode* (racing): the anchor follows the car. The cabinet and ghost controls are fixed relative to the anchor.
  - *Comfort options* (see [controls.md](controls.md) §2.3) adjust how the anchor moves: cut fades, horizon lock, vignette.
- Eye poses = anchor × head pose × eye offset, sent to the backend in game-world units. Scale is set per game: `world_units_per_metre`.

### 2.3a How backends produce stereo: three proven techniques

Studied from source, 2026-10-08:

| Technique | Used by | How | Quality |
|---|---|---|---|
| **Unproject reconstruction** | DR-89 Time Crisis VR, VC2VR | Take the game's projected polygons (screen x, y plus view-space depth z, plus that polygon's camera: focal, cx, cy). Rebuild view-space vertices with `x = (sx - cx)·z/f`, `y = (cy - sy)·z/f`. Render the rebuilt triangles per eye with OpenXR view and projection. | True 6DoF stereo; only what the game drew is visible. VC2VR re-runs the world pass with the view rotated to the 6 cube faces to fill in what's behind you. |
| **Source-level host** | DR-89 on namco22-decompile | The engine exposes a host API (`ss22_host.h`) and splits a frame into `ss22_prepare()` (once per simulation step) and `ss22_draw()` (any number of times). The host draws once per eye (or with multiview) from one simulation step. | Best. No hooking; upstream patched only through guarded build-time edits (`patch_upstream.py`). |
| **Clip-space disparity + camera RAM writes** | PenguinScreen2 (PCSX2) | Per-vertex horizontal offset in the GS vertex shader, from the PS2 per-vertex Q (1/w) value. Head rotation written into the game's camera variables in emulated RAM. | Cheap. Stereo depth only as good as Q is as a depth proxy. Head rotation needs a per-game RAM profile. Not real 6DoF parallax. |

**Generalisation for AladdinsCastle:** unproject reconstruction works with any emulator or port that can give us per-polygon screen coordinates, view-space depth and projection parameters. That's the generic route for Model 2/3, NAOMI-class and PC-port backends: hook the point where the emulated geometry engine outputs projected vertices. For PS2-class hardware with programmable vertex units, the PenguinScreen2 approach is the realistic first step.

Gun aim in both shipping gun ports matches [controls.md](controls.md) §1.1: cast the ray against the rebuilt triangles, then project the hit through *that triangle's* game camera. Time Crisis VR writes the result straight into the arcade gun register (`x = 68 + nx·626`, `y = 43 + ny·241`). VC2VR has to steer the PC game's joystick-velocity input in a closed loop, which lags. Direct register or port writes are the target.

### 2.4 Gun mapping

Covered in [controls.md](controls.md) §1.1. The backend gets an already-projected screen point and an off-screen flag, so it does not need to know about VR.

### 2.5 HUD

Arcade HUDs are 2D sprites on top of the 3D frame. Backends should put the HUD on a separate layer if they can. The hall shows that layer at a comfortable fixed depth, as a head-locked panel or a panel fixed to the cabinet, whichever the user chooses. When the backend can't separate it, the HUD stays merged into the frame at screen depth.

## 3. Backend plan

| Tier | Backend | Games | Route | Notes |
|---|---|---|---|---|
| A | **namco22-vr** (ours) | Time Crisis, Rave Racer, Ace Driver, Dirt Dash, Prop Cycle, Tokyo Wars, Cyber Sled, Cyber Commando | Fork/extension of namco22-decompile (MIT) with an ACBP host; stereo through the geo_hw.c projection | Same base as DR-89 Time Crisis VR. Renderer accuracy is still being worked on upstream. |
| A | **supermodel-vr** (ours) | Sega Model 3: Scud Race, Daytona USA 2, Sega Rally 2, Le Mans 24, The Lost World, ... | Fork of Supermodel (GPL) adding per-eye frustum in New3D (`CNew3D::CalcViewport`) + ACBP output | Clean renderer seam. Model 3 gun list to confirm. |
| A | **pcsx2-vr** | PS2 GunCon 2 games: Time Crisis 2/3, Crisis Zone, Vampire Night, Virtua Cop Elite Edition, ...; PS2 racers | Build on PenguinScreen2's stereo approach (GPL); add ACBP or a GunCon 2 aim path | PenguinScreen2 is Linux-only today |
| B | **hand-off** | Time Crisis (DR-89), Virtua Cop 2 PC (VC2VR), PenguinScreen2 titles | Launch the existing app | Day-one value |
| C | **theatre** | Everything else: Model 2 Emulator, MAME, TeknoParrot (Lindbergh, RingEdge), Flycast, Dolphin, RPCS3, DuckStation | Window capture + input bridge | |
| R | Research | Model 2 (MAME TGP / sm2-emu), Lindbergh (lindbergh-loader GL interposer), Daytona XBLA recomp, OutRun 2006 (Tweaks/decomp), Dolphin (free-look/stereo) | Find each one's stereo seam | See [landscape.md](landscape.md) |

## 4. Input bridge (theatre and hand-off backends)

Ordered by preference:

1. **Emulator-native APIs.** No device needed and per-player by design: MAME Lua plugins, a DemulShooter-style memory feed (DemulShooter already supports many Model 2/3, Lindbergh, RingWide and TeknoParrot gun games), and IPC APIs where emulators have them.
2. **vJoy (signed Brunner fork 2.2.x)** for wheels, pedals and analog axes, with HidHide to hide the real devices from games. Not ViGEmBus, which was archived in 2023.
3. **Absolute mouse in the window** for single-gun theatre games.
4. **Custom VHF HID driver** only if two absolute guns are needed at device level. It's a kernel driver, so it needs attestation signing, so it comes last.

Outputs (recoil, lamps, FFB) come back through MAMEHooker-style output events or DemulShooter and become controller haptics.

## 5. Platforms

- **v1: Windows x64 + PCVR.** Quest 3 through Quest Link (cable, reference), Air Link or Virtual Desktop (VDXR). Any OpenXR runtime should work, with SteamVR supported. Graphics API: Vulkan or D3D11 for the hall, decided after testing interop against real runtimes (**OPEN**).
- **Later: Quest standalone (Android, OpenXR, Vulkan).** Godot exports to Quest. The namco22 host is C and DR-89 already runs it on Quest, so System 22 games are the natural first standalone titles. Emulator backends stay PC-only.
- **Linux/SteamOS:** desirable (PenguinScreen2 and lindbergh-loader are Linux-native). Not a v1 target.

## 6. Repository layout (planned)

```
aladdinscastle/
  docs/                 design docs (this folder)
  hall/                 Godot 4 project (front end)
  native/               GDExtension: compositor, ACBP client, capture, input bridge
  backends/
    namco22-vr/         ACBP host on namco22-decompile (submodule + patches)
    supermodel-vr/      fork notes/patches
  protocol/             ACBP spec + C header + test backend ("spinning cube" stereo backend)
  data/                 built-in config: games, backends, controls, cabinets, halls
  schemas/              JSON Schemas for each config kind
  tools/                CLI: validate, explain, hash-check, pack
```
