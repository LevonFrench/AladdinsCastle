# Front end: the hall

The front end is a VR arcade you walk around. It's also the launcher, the settings screen, the art browser and the place your scores live.

## 1. Spaces

| Space | Purpose |
|---|---|
| **Hall** | The main room. Rows of cabinets laid out by `hall.toml` rules. Ambient attract-mode audio, neon, carpet. |
| **Cabinet** | A walk-up machine: 3D model, marquee, side art and a live attract video on its screen, plus a holster or seat. Interacting with it opens the cabinet card. |
| **Cabinet card** | A floating panel next to the cabinet: title, year, maker, board, players, chosen backend and its quality badge (True 3D / Hand-off / Theatre), media status (found / missing / bad hash), high scores, settings for that game. |
| **Game** | The backend runs. True-3D games replace the hall. Theatre games show inside the hall on the cabinet screen or on a big screen. |
| **Workshop** | The settings room: calibration (height, gun angle, hand), bindings, comfort, paths, backend installs, and pack management. |
| **List mode** | A flat, searchable 2D panel of every game. Always one button away, for when you don't want to walk. |

## 2. Cabinet presentation

- **Branded cabinets** come from packs as glTF with named slots (see [config-spec.md](config-spec.md) §6). Original geometry only. We can't redistribute manufacturer models, and no CC-licensed Time Crisis or Daytona cabinet model has been found so far.
- **Generic cabinets** are built from parameters: upright, sit-down twin, deluxe gun (with pedal), motorbike and so on. The game's own art goes into their slots, so every game has a cabinet from the first day.
- **Attract mode**: a short video clip if the user has one, otherwise a live low-rate run of the backend in theatre mode. **OPEN**: is that affordable on a Quest-streamed PC?
- **Coin-up animation, marquee lighting, CRT glass and bezel** are all cabinet-level settings.

## 3. Art and metadata pipeline

Rules:
1. **We ship no third-party art.** Packs may only include art they have the right to distribute (original works, CC-licensed works, community-made art with permission).
2. **Scraping happens on the user's machine,** with the user's own accounts where a service requires them. Every downloaded asset gets a sidecar `*.source.toml` recording where it came from, when, and under what terms.
3. **Everything falls back** to generated art: title text on a marquee template, coloured by maker.

Source candidates (terms must be confirmed per source before we integrate it):

| Source | What | Status |
|---|---|---|
| ScreenScraper | Marquees, flyers, videos, screenshots | Has an API with daily quotas. Redistribution terms unclear. User-side scraping only. |
| Arcade Database (ADB) | Arcade metadata, images | Terms not found yet |
| progettosnaps | Marquees, cabinets, flyers, control panels, bezels | Not yet reviewed |
| MAME history.xml | Game history text | Terms to confirm |
| LaunchBox Games DB / IGDB / TheGamesDB | Console metadata | Terms not confirmed |

## 4. Front-end config model

Learn from existing front ends:
- **Pegasus** `metadata.pegasus.txt`: plain-text and human-editable, with `x-*` custom keys. That matches our "unknown keys pass through" rule.
- **Batocera** `es_systems_<name>.cfg` overlays: users override only what changed. That matches our layered merge.
- **EmuVR**: imported user-generated models plus a generic placeholder for anything not modelled. That matches our generic cabinets.

## 5. Scores, profiles and stats

- Local high-score boards per game, read from backend outputs where available (decomp hosts can report score directly; emulators through hiscore plugins).
- Player profiles with height, handedness and comfort settings.
- Play time and play counts, stored in `user/` as plain files.

## 6. Engine choice (OPEN, recommendation)

| Option | For | Against |
|---|---|---|
| **Godot 4 (recommended)** | MIT; first-class OpenXR on PC and Quest-native Android; easy to mod; scenes and resources in text formats; GDExtension for native C++ | XR ecosystem smaller than Unity's |
| Unity | Biggest XR ecosystem; Meta Interaction SDK (grab poses) | Licence and runtime-fee history; not friendly to an open-source, mod-first project |
| Custom C++ / OpenXR | Full control; lowest overhead | Every UI and scene tool has to be built by us |

Recommendation: **Godot 4 for the hall and UI. Backend and compositor glue as C++ GDExtension plus separate backend processes** (see [architecture.md](architecture.md)).
