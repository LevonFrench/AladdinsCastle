# Game packages: one folder per game, like one mod in the Installer Hub

Each game in AladdinsCastle exists the way each mod exists in [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub): a self-contained folder with its catalog entry, installer, README and art. The Hub shows it as a tile and a detail page with **Install**, **Start in VR**, **Update**, **Reinstall** and **Uninstall**. *(Owner decision 2026-10-08.)*

| Installer Hub (per mod) | AladdinsCastle (per game) |
|---|---|
| Entry in `Core/Modules/Catalog.ps1` (Title, SteamId, Mod, Author, Controls, Roomscale, VideoUrl, ModFile, Color, Tags...) | `games/<id>/game.toml` |
| `Core/<Game>VR/<Game>VR-core.ps1` + `START_INSTALLER.bat` | `games/<id>/install.toml` (declarative), optional `install.ps1` for odd cases |
| `README_<Game>VR.md` | `games/<id>/README.md` |
| `ModFile` = proof the mod is installed | `installed_when` in `install.toml` |
| Uninstall guide | `[uninstall]` steps in `install.toml` (generated from install steps where possible) |
| Banner and colours from Steam ID | `games/<id>/art/` (user-scraped or pack art), colour and accent in `game.toml` |

## 1. Folder

```
games/timecris/
  game.toml        # tile + detail page: title, badges, credits, colours, tags, links
  install.toml     # one or more VR setups ("variants") and how to install each
  README.md        # shown on the detail page: features, how to use, controls, requirements
  art/             # tile.png, banner.png, marquee.png, flyer.jpg, preview.mp4 (optional)
  setup/           # our config shipped for this game: controls.toml, gun.toml, comfort.toml
  install.ps1      # optional escape hatch
```

Installed files go to `installed/<game-id>/<variant>/`. Shared emulators and tools go to `tools/` (see [frontend.md](frontend.md) §4). User settings go to `user/`.

## 2. Tile and detail page

**Badges** (from `game.toml` and the chosen variant): `TRUE 3D` / `THEATRE` · `GUN` / `WHEEL` / `HANDLEBARS` / `BIKE` · `ROOMSCALE` / `SEATED` · `2 PLAYERS` · `WIP` · `QUEST STANDALONE`.

**States and the main button:**

| State | Shown when | Main button |
|---|---|---|
| Needs your files | Required media not found | **Find my files** (point or search) |
| Needs an emulator | Required tool missing | **Install / locate emulator** |
| Ready to install | Everything found, setup not installed | **Install** |
| Installed | `installed_when` is true | **Start in VR ▶** |
| Update available | Upstream has a newer version than recorded | **Update** |
| Coming soon | Variant is `planned` | Link to the roadmap item |

**Detail page sections:** description · README · what it installs (with credits and links to each upstream) · requirements · controls (generated from the control set) · settings for this game (cover mode, laser, gun angle, comfort) · variant picker if more than one · similar games · uninstall.

## 3. `game.toml`

```toml
id       = "timecris"
title    = "Time Crisis"
year     = 1995
maker    = "Namco"
board    = "Namco Super System 22"
genre    = ["gun"]
players  = 1
badges   = ["gun", "roomscale", "cover"]
tags     = ["namco", "system-22", "rail shooter", "pedal"]
colour   = "#1a0f0f"
accent   = "#e0302a"
blurb    = "Duck behind cover for real, then pop up and shoot."
video    = ""                        # optional preview link
links    = { wiki = "https://en.wikipedia.org/wiki/Time_Crisis_(video_game)" }

[media]                              # the user's own files, located or searched, checked by hash
timecris = { file = "timecris.zip", kind = "mame-romset", note = "Time Crisis World TS2 Ver.B" }
```

## 4. `install.toml`

A game can have several **variants**. The Hub picks the best one available and lets the user switch.

```toml
default = "namco22-vr"

[variant.namco22-vr]
title    = "AladdinsCastle true 3D (namco22-decompile + libacvr)"
quality  = "true3d"
status   = "planned"                 # planned | wip | stable
needs    = { media = ["timecris"], tools = [] }

[variant.dr89]
title    = "Time Crisis VR by DR-89 (PCVR)"
quality  = "true3d"
status   = "stable"
needs    = { media = [], tools = [] }
installed_when = "${install_dir}/TimeCrisisVR.exe"
steps = [
  { do = "locate-package", what = "Time Crisis VR Windows build (ROM-free)", pattern = "TimeCrisisVR-*-windows-x64*.zip", note = "See README: the upstream release zip bundles game files, so the Hub does not download it. Point to a ROM-free build." },
  { do = "extract", to = "${install_dir}" },
  { do = "copy-media", media = "timecris", to = "${install_dir}/roms/" },
  { do = "write-config", file = "${install_dir}/quest-options.cfg", set = { physical_crouch = "${settings.cover == 'duck'}", left_handed = "${profile.left_handed}" } },
  { do = "shortcut", targets = ["steam"], exe = "${install_dir}/Play SteamVR.cmd" },
]
launch = { exe = "${install_dir}/Play SteamVR.cmd", cwd = "${install_dir}" }
```

Step kinds are listed in [frontend.md](frontend.md) §3, plus `locate-package` (the user points to a file they obtained) and `copy-media` (copies or links the user's own checked files into place).

## 5. README.md template

Same sections as the Installer Hub's per-mod READMEs, which users already know:

1. One-paragraph description
2. Features
3. How to use (Install, Start in VR, recenter)
4. Controls (generated table, plus notes)
5. What it installs (each component, version, author, link)
6. Requirements (your files, emulator, VR runtime)
7. Settings and hotkeys
8. Troubleshooting
9. Credits

## 6. Examples in this repo

- [`games/timecris/`](../games/timecris/): DR-89 variant (stable), our namco22-vr (planned)
- [`games/vcop2/`](../games/vcop2/): VC2VR variant on the 1997 PC release (stable)
- [`games/raverace/`](../games/raverace/): our namco22-vr with ghost wheel and shifter (planned)
- [`games/scud/`](../games/scud/): our supermodel-vr (planned)
