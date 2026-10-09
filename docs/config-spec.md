# Configuration specification (draft 1)

Everything AladdinsCastle knows about a game, its VR setups, install recipes, control sets and guns comes from plain-text files. There is no compiled-in game list. This document defines those files.

Status: **draft for discussion**. Field names will change before v1. Open questions are marked **OPEN**.

## 1. Principles

1. **Open-ended.** Unknown keys are kept, not rejected. They pass through to the setup or plugin that understands them. A game file can carry fields that only one setup reads.
2. **One thing per file, one folder per thing.** A game is `games/<id>/game.toml` plus its art, next to it. Copy the folder and you have shared the game.
3. **Layered.** Built-in defaults, then installed packs, then user overrides. Later layers deep-merge over earlier ones (see §8).
4. **Validated, not locked.** Each kind has a JSON Schema in `schemas/`. Validation warns about typos and wrong types but never blocks unknown keys.
5. **Human first.** TOML is the canonical format, with comments encouraged. JSON with the same data model is accepted for generated files. **OPEN:** also accept YAML, which PenguinScreen2 uses for its profiles?
6. **Stable IDs.** Lowercase `kebab-case`, globally unique within their kind. Use MAME set names where one exists (`timecris`, `raverace`) so files line up with existing tools.

## 2. Folder layout

```
<AladdinsCastle folder>/          # portable: everything lives here
  config.toml              # global settings (paths, VR runtime, comfort defaults)
  games/<game-id>/
    game.toml
    art/                   # marquee.png, flyer.jpg, screenshots, video (user-scraped or pack art)
  setups/<setup-id>/
    setup.toml             # how to launch one VR setup
    ...                    # files the recipe installed (third-party releases, our builds)
  recipes/<recipe-id>.toml # how to install a setup (see frontend.md section 3)
  tools/<tool-id>.toml     # emulator/helper: install, locate and search rules (frontend.md section 4)
  tools/<tool-id>/         # tools the Hub installed for you
  controls/<control-set-id>.toml
  guns/<gun-id>/gun.toml + model.glb
  packs/<pack-id>/pack.toml (+ any of the folders above)
  user/
    overrides/...          # same layout; wins over everything
    bindings.toml
    profiles/<player>.toml
    scores/, saves/, calibration/
  media/                   # default place for the user's own ROMs/ISOs; never shipped
```

All paths may use variables: `${data}`, `${media}`, `${game_dir}`, `${setup_dir}`, `${env:NAME}`.

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

# VR setups in preference order. The Hub offers the first one that is installed
# and has its media; the user can pick another on the game page.
[[setups]]
id      = "namco22-vr"            # our true-3D setup (libacvr on namco22-decompile)
recipe  = "timecris-namco22-vr"
quality = "true3d"

[[setups]]
id      = "time-crisis-vr-dr89"   # existing third-party VR port
recipe  = "timecris-dr89"
quality = "true3d"

[[setups]]
id      = "mame-theatre"          # fallback: virtual screen
recipe  = "mame-theatre"
quality = "theatre"
args    = { set = "timecris" }

[controls]
set       = "gun-1p-pedal"        # controls/gun-1p-pedal.toml
gun_model = "guncon-arcade"       # guns/guncon-arcade/
cover     = "duck"                # overrides the control set default

[gun.calibration]                 # mapping from projected screen space to device range
x = { min = 0.0, max = 1.0, out_min = 0, out_max = 1023 }
y = { min = 0.0, max = 1.0, out_min = 0, out_max = 1023 }

[art]                             # paths relative to game_dir, or scraper refs
marquee = "art/marquee.png"
flyer   = "art/flyer.jpg"
video   = "art/attract.mp4"
steam   = { grid = "art/steam-grid.png", hero = "art/steam-hero.png" }   # for SteamVR library shortcuts

[comfort]
camera_cuts = "fade"              # game has hard cuts between cover positions

[ext.wiki]                        # anything under ext.* is free-form
notes = "Ver.B (TS2) is the set the decomp expects."
```

## 4. `setup.toml`

A setup is one way of running a game in VR. Three kinds:

| `kind` | Meaning | Example |
|---|---|---|
| `acvr` | Our true-3D setup: an engine or emulator built with libacvr | namco22-vr, supermodel-vr |
| `external-vr` | A third-party VR app, installed and configured by a recipe, run as-is | DR-89 Time Crisis VR, VC2VR, PenguinScreen2 |
| `theatre` | acvr-theatre plus a flat emulator, with an input bridge | MAME, Model 2 Emulator, TeknoParrot, Dolphin |

```toml
id        = "time-crisis-vr-dr89"
name      = "Time Crisis VR (DR-89)"
kind      = "external-vr"
upstream  = "https://github.com/DR-89/time-crisis-vr"
license   = "MIT (port code)"
platforms = ["windows-x64", "quest-android"]

[launch.windows-x64]
exe  = "${setup_dir}/Play SteamVR.cmd"
cwd  = "${setup_dir}"

[launch.quest-android]
package = "?"                        # OPEN: read the real package id from the APK

[capabilities]
stereo = true
gun    = "internal"                  # the app does its own aiming

[detect]
files = ["${setup_dir}/Play VR.cmd"]
```

`acvr` setups also declare which libacvr modules they use (`gun`, `ghost`, `cover`), the outputs they emit (`ffb`, `lamps`, `recoil`), and whether they report the game camera every frame (see [architecture.md](architecture.md) §2).

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

## 6. Recipes

Install recipes are defined in [frontend.md](frontend.md) §3. A recipe installs one setup for one or more games. It downloads only from original sources, checks hashes, and never fetches game content.

## 7. User bindings and profiles

`user/bindings.toml` maps OpenXR actions to control set elements, per controller type:

```toml
[profile."/interaction_profiles/meta/touch_controller_plus"]
fire         = "/user/hand/*/input/trigger/value"
grab         = "/user/hand/*/input/squeeze/value"
reload       = "/user/hand/right/input/a/click"
recenter     = "chord:/user/hand/left/input/squeeze/click+/user/hand/right/input/squeeze/click"
```

`user/profiles/<player>.toml` holds handedness, standing height, IPD offsets, comfort settings and per-game overrides (gun angle, laser on/off).

## 8. Layering and merge rules

Load order: `built-in` → `packs/*` (alphabetical, or by `priority` in `pack.toml`) → `user/overrides`.

- Tables merge recursively.
- Scalars: the later layer wins.
- Arrays of tables with an `id` field merge by `id`. Without `id`, the later array replaces the earlier one.
- `"!delete"` as a value removes a key inherited from an earlier layer.
- `aladdinscastle config explain <game-id>` (planned CLI) prints the merged result with the file each value came from.

## 9. Packs: `pack.toml`

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

## 10. Open questions

- **OPEN:** TOML only, or TOML + YAML + JSON?
- **OPEN:** Hash database format for media verification. Reuse MAME software list XML hashes, or our own `hashpack`?
- **OPEN:** Script hooks: shell commands only, or embedded Lua (as MAME and Model 2 Emulator use)?
- **OPEN:** Recipe scripts: allow PowerShell steps (as the Installer Hub does), or only declarative steps plus vetted helper exes?
