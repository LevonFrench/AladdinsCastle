# Game packages: one folder per game, like one mod in the Installer Hub

Each game in AladdinsCastle exists the way each mod exists in [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub): a self-contained folder with its catalog entry, installer, README and art. The Hub shows it as a tile and a detail page with **Install**, **Start in VR**, **Update**, **Reinstall** and **Uninstall**. *(Owner decision 2026-10-08.)*

| Installer Hub (per mod) | AladdinsCastle (per game) |
|---|---|
| Entry in `Core/Modules/Catalog.ps1` (Title, SteamId, Mod, Author, Controls, Roomscale, VideoUrl, ModFile, Color, Tags...) | `games/<id>/game.toml` |
| `Core/<Game>VR/<Game>VR-core.ps1` + `START_INSTALLER.bat` | `games/<id>/install.toml` (declarative), optional `install.ps1` for odd cases |
| `README_<Game>VR.md` | `games/<id>/README.md` |
| `ModFile` = proof the mod is installed | `installed_when` in `install.toml` |
| Uninstall guide | `[uninstall]` steps in `install.toml` (generated from install steps where possible) |
| Banner and colours from Steam ID | `user/art/<id>/` (user-scraped, private) or `games/<id>/art/` (licensed pack art), colours in `game.toml` `[hub]` |

## 1. Folder

```
games/timecris/
  game.toml        # tile + detail page: title, badges, credits, colours, tags, links
  install.toml     # one or more VR setups ("variants") and how to install each
  README.md        # shown on the detail page: features, how to use, controls, requirements
  art/             # only redistributable art (original / licensed); scraped art goes to user/art/<id>/
  setup/           # our config shipped for this game: controls.toml, gun.toml, comfort.toml
  install.ps1      # optional escape hatch
```

**Art location rule:** `games/<id>/art/` holds only art the repo or a pack is allowed to redistribute (original or properly licensed). Art the Hub scrapes or the user adds goes to `user/art/<id>/`, which is gitignored and never published. The Hub looks in `user/art/<id>/` first, then `games/<id>/art/`, then generates a fallback.

Installed files go to `installed/<game-id>/<variant>/`. Shared emulators and tools go to `tools/` (see [frontend.md](frontend.md) §4). User settings go to `user/`.

## 2. Tile and detail page

**Badges** (from `game.toml` and the chosen variant): `TRUE 3D` / `THEATRE` · `GUN` / `WHEEL` / `HANDLEBARS` / `BIKE` · `ROOMSCALE` / `SEATED` · `2 PLAYERS` · `WIP` · `QUEST STANDALONE`.

**States and the main button:**

This is the single list of game states; other docs refer to it.

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

Every field, the allowed values and the `[hub]` card block are defined in [game-schema.md](game-schema.md). Real example: [`games/timecris/game.toml`](../games/timecris/game.toml).

## 4. `install.toml`

A game can have several **variants**. The Hub picks the best one available and lets the user switch.

```toml
default = "dr89-pcvr"

[variant.namco22-vr]
title   = "AladdinsCastle true 3D (namco22-decompile + libacvr)"
quality = "true3d"
status  = "planned"                     # planned | wip | stable
needs   = { media = ["timecris"], tools = [] }

[variant.dr89-pcvr]
title    = "Time Crisis VR by DR-89 (PCVR)"
quality  = "true3d"
status   = "stable"
needs    = { media = ["timecris"], tools = [] }
installed_when = "${install_dir}/TimeCrisisVR.exe"

[[variant.dr89-pcvr.step]]
do      = "locate-package"              # upstream zip bundles game files: never downloaded by the Hub
what    = "Time Crisis VR Windows build without game files"
pattern = "TimeCrisisVR-*-windows-x64*.zip"

[[variant.dr89-pcvr.step]]
do = "extract"
to = "${install_dir}"

[[variant.dr89-pcvr.step]]
do    = "copy-media"
media = "timecris"
to    = "${install_dir}/roms/"

[variant.dr89-pcvr.launch]
exe = "${install_dir}/Play SteamVR.cmd"
cwd = "${install_dir}"
```

Full file: [`games/timecris/install.toml`](../games/timecris/install.toml). This `[[variant.<id>.step]]` form is the only install format.

Step kinds are listed in [frontend.md](frontend.md) §3.

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
