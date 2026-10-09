# Game catalog schema (`games/<id>/game.toml`)

Every game is a folder (see [game-packages.md](game-packages.md)). This page fixes the fields so the Hub can **filter by genre, manufacturer, year and hardware** (and more). Values for `genre`, `subgenre`, `manufacturer`, `hardware` and `graphics` must come from the vocab files in [`data/vocab/`](../data/vocab/). New values are added to the vocab file first, in the same change.

## 1. One entry per release

- **Arcade game**: one entry per game, using the MAME parent set name as the `id` (`timecris`, `vcop2`, `scud`). Regional or revision sets are listed in `media`, not as separate entries.
- **Console or PC release**: one entry per platform release, with `id = "<platform>-<slug>"` (`ps2-time-crisis-2`, `dc-confidential-mission`, `wii-ghost-squad`). Ports of an arcade game point back to it with `original = "<arcade id>"`, so the Hub can show "Also on PS2 / Dreamcast...".
- Compilations get their own entry, with `contains = [...]` (`ps2-virtua-cop-elite-edition` contains `vcop`, `vcop2`).
- PC releases that a VR mod targets (e.g. Virtua Cop 2 PC for VC2VR) are listed as **alternative media of the arcade entry**, not as separate games.

## 2. Fields

```toml
id           = "timecris"                 # required; see §1
title        = "Time Crisis"              # required; common English title
alt_titles   = []                         # other regional titles
genre        = "gun"                      # required; vocab/genres.toml: gun | racing
subgenre     = ["rail-shooter", "cover-shooter"]   # vocab/genres.toml
year         = 1995                       # required; first release of THIS release
manufacturer = "namco"                    # required; vocab/manufacturers.toml (brand on cabinet/box)
developer    = "Namco"                    # free text; studio if different (e.g. "Sega AM2", "Wow Entertainment")
hardware     = "namco-super-system-22"    # required; vocab/hardware.toml
graphics     = "polygon-3d"               # required; vocab/graphics.toml: polygon-3d | mixed | sprite-scaler | 2d (FMV games are out of scope)
players      = 1                          # max simultaneous players
series       = "Time Crisis"              # optional
original     = ""                         # ports: id of the arcade original
contains     = []                         # compilations: ids included
regions      = ["world", "jp", "us"]      # optional

[controls]                                # what the original cabinet/peripheral had
type    = "gun"                           # gun | wheel | handlebars | bike | ski | joystick | yoke | boat | other
guns    = 1                               # gun games: guns per cabinet
gun     = "Namco blue gun (recoil)"       # free text
shifter = ""                              # racing: none | hl | h4 | h6 | seq
pedals  = ["cover-pedal"]                 # gun: cover-pedal; racing: accel, brake, clutch
extras  = []                              # e.g. view-change button, nitro, sniper scope, shake

[[media]]                                 # what the user must own (never downloaded)
kind  = "mame-romset"                     # mame-romset | disc | pc-game | bios | other
set   = "timecris"                        # MAME set; or serial = "SLUS-20219" for discs
note  = "World TS2 Ver.B"

[routes]                                  # how it can run today (status vocab below)
mame        = "imperfect"
teknoparrot = "profile"
native      = { project = "namco22-decompile", status = "playable" }
vr          = { best = "true3d", via = "DR-89 Time Crisis VR" }   # best: true3d | theatre | none; planned: true3d-planned

[hub]                                     # how the tile looks (Mod Hub style card)
pill   = "TIMECRIS"                       # short tag in the card's top-left pill (uppercase, <= 12 chars)
colour = "#1a0f0f"                        # card background tint
accent = "#e0302a"                        # border glow, version line, Install button
blurb  = "Duck behind cover for real, then pop up and shoot."   # one line under the author
badges = ["roomscale", "quest-standalone"]                       # extra tile badges

[meta]
sources    = ["https://github.com/mamedev/mame/blob/master/src/mame/namco/namcos22.cpp"]
confidence = "high"                       # high | medium | low (how well-sourced this entry is)
checked    = "2026-10-08"
```

`[routes]` status values: `working`, `imperfect`, `not-working`, `profile` (TeknoParrot profile exists), `playable`, `unknown`. Emulator keys: `mame`, `supermodel`, `model2emu`, `flycast`, `redream`, `demul`, `dolphin`, `pcsx2`, `duckstation`, `rpcs3`, `mednafen`, `yabasanshiro`, `mupen64plus`, `ares`, `xemu`, `cxbx`, `teknoparrot`, `demulshooter` (gun bridge exists: `profile`), `lindbergh-loader`, `xenia`, `cemu`, `ryujinx`, `native`.

## 3. Hub filters

| Filter | Field | UI |
|---|---|---|
| Genre | `genre`, `subgenre` | chips |
| Manufacturer | `manufacturer` | multi-select |
| Year | `year` | range slider + decade chips |
| Graphics | `graphics` | chips: 3D / Mixed / Sprite scaler / 2D |
| Hardware | `hardware` (grouped by `family` and `kind` from the vocab) | tree: Arcade → Sega → Model 3; Console → Sony → PS2 |
| VR | `routes.vr.best` | True 3D / Theatre / Planned |
| Players | `players` | 1 / 2+ |
| Controls | `controls.type` | gun, wheel, bike... |
| In my library | media found by the scanner | toggle |
| Status | install state | Installed / Ready / Needs files |

Sorting: title, year, manufacturer, hardware, recently played.
