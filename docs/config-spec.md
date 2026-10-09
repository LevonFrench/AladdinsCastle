# Configuration specification (draft 0)

Everything AladdinsCastle knows about a game, backend, cabinet, control set or hall comes from plain-text files. There is no compiled-in game list. This document defines those files.

Status: **draft for discussion**. Field names will change before v1. Open questions are marked **OPEN**.

## 1. Principles

1. **Open-ended.** Unknown keys are kept, not rejected. They pass through to the backend or plugin that understands them. A game file can carry fields that only one backend reads.
2. **One thing per file, one folder per thing.** A game is `games/<id>/game.toml` plus its art, next to it. Copy the folder and you have shared the game.
3. **Layered.** Built-in defaults, then installed packs, then user overrides. Later layers deep-merge over earlier ones (see §9).
4. **Validated, not locked.** Each kind has a JSON Schema in `schemas/`. Validation warns about typos and wrong types but never blocks unknown keys.
5. **Human first.** TOML is the canonical format, with comments encouraged. JSON with the same data model is accepted for generated files. **OPEN:** also accept YAML, which PenguinScreen2 uses for its profiles?
6. **Stable IDs.** Lowercase `kebab-case`, globally unique within their kind. Use MAME set names where one exists (`timecris`, `raverace`) so files line up with existing tools.

## 2. Folder layout

```
<data root>/
  config.toml              # global settings (paths, runtime, comfort defaults)
  games/<game-id>/
    game.toml
    art/                   # marquee.png, flyer.jpg, screen-attract.mp4, ...
  backends/<backend-id>/
    backend.toml
  cabinets/<cabinet-id>/
    cabinet.toml
    model.glb
    art/                   # side-art, bezel, control-panel textures
  controls/<control-set-id>.toml
  guns/<gun-id>/gun.toml + model.glb
  halls/<hall-id>/hall.toml (+ assets)
  themes/<theme-id>/theme.toml
  packs/<pack-id>/pack.toml (+ any of the folders above)
  user/
    overrides/...          # same layout; wins over everything
    bindings.toml
    profiles/<player>.toml
    scores/, saves/, calibration/
  media/                   # the user's own ROMs/ISOs/PC games; never shipped
```

All paths may use variables: `${data}`, `${media}`, `${game_dir}`, `${backend_dir}`, `${env:NAME}`.

## 3. `game.toml`

```toml
id      = "timecris"
title   = "Time Crisis"
year    = 1995
maker   = "Namco"
genre   = ["gun"]             # gun | racing | ... (free-form tags allowed)
tags    = ["namco", "system-22", "rail", "cover"]
players = 1
board   = "namco-super-system-22"

[media]                        # what the user must supply; never downloaded
main = { file = "timecris.zip", kind = "mame-romset", set = "timecris", mame_min = "0.271" }
# Per-chip hashes may be listed or imported from a hash pack:
hashes = "hashpack:namco22/timecris"

# Backends in preference order. The first one that is installed and has its media wins.
[[backends]]
id      = "namco22-vr"            # true-3D native host (our backend on namco22-decompile)
quality = "true3d"

[[backends]]
id      = "time-crisis-vr-dr89"   # existing third-party VR port, launched in hand-off mode
quality = "true3d"

[[backends]]
id      = "mame-theatre"          # fallback: virtual screen
quality = "theatre"
args    = { set = "timecris" }

[controls]
set       = "gun-1p-pedal"        # controls/gun-1p-pedal.toml
gun_model = "guncon-arcade"       # guns/guncon-arcade/
cover     = "duck"                # overrides the control set default

[gun.calibration]                 # mapping from projected screen space to device range
x = { min = 0.0, max = 1.0, out_min = 0, out_max = 1023 }
y = { min = 0.0, max = 1.0, out_min = 0, out_max = 1023 }

[cabinet]
id = "namco-time-crisis-dx"       # cabinets/namco-time-crisis-dx/

[art]                             # paths relative to game_dir, or scraper refs
marquee = "art/marquee.png"
flyer   = "art/flyer.jpg"
attract = "art/attract.mp4"

[comfort]
camera_cuts = "fade"              # game has hard cuts between cover positions

[ext.wiki]                        # anything under ext.* is free-form
notes = "Ver.B (TS2) is the set the decomp expects."
```

## 4. `backend.toml`

A backend is anything that can run a game. Four kinds:

| `kind` | Meaning | Example |
|---|---|---|
| `acbp` | Speaks the AladdinsCastle Backend Protocol: renders stereo for poses we send and accepts game input from us. Best integration. | Our namco22 host; our Supermodel and PCSX2 stereo plugins |
| `openxr-handoff` | A separate VR app that owns the headset while it runs. We launch it, hand over, and come back when it exits. | DR-89 Time Crisis VR, VC2VR, PenguinScreen2 |
| `theatre` | A flat emulator window, captured and shown on a virtual screen. Aim goes through the input bridge. | MAME, Model 2 Emulator, TeknoParrot, Dolphin |
| `script` | Custom launch logic in a script file | Odd PC ports |

```toml
id      = "time-crisis-vr-dr89"
name    = "Time Crisis VR (DR-89)"
kind    = "openxr-handoff"
upstream = "https://github.com/DR-89/time-crisis-vr"
license  = "MIT (port code)"
platforms = ["windows-x64", "quest-android"]

[launch.windows-x64]
exe  = "${backend_dir}/bin/TimeCrisisVR.exe"   # user installs the release here
args = []
cwd  = "${backend_dir}/bin"

[launch.quest-android]
package = "com.dr89.timecrisisvr"               # OPEN: verify the real package id

[capabilities]
stereo = true
gun    = "internal"            # the app handles its own aiming
return = "on-exit"             # back to the hall when the process exits

[install]
detect = ["${backend_dir}/bin/TimeCrisisVR.exe"]
help   = "Download the Windows zip from the Releases page and extract it into bin/."
```

ACBP backends also declare `protocol_version`, the input devices they accept (`gun`, `wheel`, `buttons`), outputs (`ffb`, `lamps`, `recoil`), and whether they provide depth and the game camera per frame (see [architecture.md](architecture.md)).

## 5. Control sets: `controls/<id>.toml`

```toml
id    = "namco-rave-racer"
title = "Rave Racer cabinet"
seat  = { height_m = 0.55, posture = "seated" }

[[element]]
id        = "wheel"
type      = "wheel"
position  = [0.0, 0.78, 0.42]      # metres, relative to the seat reference point
tilt_deg  = 25
radius_m  = 0.17
range_deg = 270
spring    = { return_speed = 6.0 }
output    = { axis = "steering", invert = false }
haptics   = { centre_detent = 0.2, ffb = "game" }

[[element]]
id     = "shifter"
type   = "shifter_hl"
position = [0.22, 0.62, 0.30]
output = { low = "gear_low", high = "gear_high" }

[[element]]
id     = "accel"
type   = "pedal"
source = "right_trigger"
curve  = { deadzone = 0.04, gamma = 1.4 }
output = { axis = "accelerator" }

[[element]]
id     = "brake"
type   = "pedal"
source = "left_trigger"
output = { axis = "brake" }

[[element]]
id     = "view"
type   = "button"
position = [-0.18, 0.70, 0.36]
output = { button = "view_change" }
```

Element `type`s are the ones listed in [controls.md](controls.md) §2.1. A new type can be added by a plugin that registers it, and older code ignores types it does not know (with a warning).

## 6. Cabinets: `cabinet.toml`

```toml
id     = "namco-time-crisis-dx"
title  = "Time Crisis (deluxe)"
model  = "model.glb"                  # glTF 2.0; named nodes are slots
footprint_m = [0.9, 1.4]

[slots]                                # glTF node name -> texture or behavior
"Marquee"   = { texture = "art/marquee.png", emissive = 1.5 }
"SideArt.L" = { texture = "art/side.png" }
"Screen"    = { role = "screen", aspect = "4:3", curvature = 0.0 }
"Pedal"     = { role = "prop" }
"Holster.R" = { role = "gun-holster", gun = "guncon-arcade" }

[audio]
attract_volume = 0.6
```

If no model exists, a *generic cabinet* is built from parameters (size, colour and art slots), so any game gets a cabinet on day one.

## 7. Halls: `hall.toml`

Rows can be fixed lists or rules:

```toml
id    = "main"
title = "Aladdin's Castle"
scene = "scenes/castle-hall.glb"

[[row]]
title = "Gun games"
rule  = { genre = "gun", sort = "year" }

[[row]]
title = "Namco System 22"
rule  = { board = "namco-*system-22" }

[[row]]
title = "Favourites"
rule  = { favourite = true }
```

## 8. User bindings and profiles

`user/bindings.toml` maps OpenXR actions to control set elements, per controller type:

```toml
[profile."/interaction_profiles/meta/touch_controller_plus"]
fire         = "/user/hand/*/input/trigger/value"
grab         = "/user/hand/*/input/squeeze/value"
reload       = "/user/hand/right/input/a/click"
recenter     = "chord:/user/hand/left/input/squeeze/click+/user/hand/right/input/squeeze/click"
```

`user/profiles/<player>.toml` holds handedness, standing height, IPD offsets, comfort settings and per-game overrides (gun angle, laser on/off).

## 9. Layering and merge rules

Load order: `built-in` → `packs/*` (alphabetical, or by `priority` in `pack.toml`) → `user/overrides`.

- Tables merge recursively.
- Scalars: the later layer wins.
- Arrays of tables with an `id` field merge by `id`. Without `id`, the later array replaces the earlier one.
- `"!delete"` as a value removes a key inherited from an earlier layer.
- `aladdinscastle config explain <game-id>` (planned CLI) prints the merged result with the file each value came from.

## 10. Packs: `pack.toml`

```toml
id       = "namco-system-22"
title    = "Namco System 22 games"
version  = "0.1.0"
authors  = ["..."]
license  = "CC-BY-4.0"         # for the config and art the pack itself contains
priority = 100
requires = { aladdinscastle = ">=0.1" }
```

Packs never contain ROMs, BIOS files or game binaries. The pack validator rejects archives containing known ROM file names or hashes.

## 11. Open questions

- **OPEN:** TOML only, or TOML + YAML + JSON?
- **OPEN:** Hash database format for media verification. Reuse MAME software list XML hashes, or our own `hashpack`?
- **OPEN:** Script hooks: shell commands only, or embedded Lua (as MAME and Model 2 Emulator use)?
- **OPEN:** Should control sets live inside cabinets, so cabinet geometry and ghost controls share one source of truth?
