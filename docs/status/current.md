# Where AladdinsCastle stands (2026-10-21)

The single current-state page. It replaces "M2 development" as the summary. Older checkpoint reports stay in [docs/status/](./) as history.

**Evidence levels** used below and in the setup READMEs:

| Level | Meaning |
|---|---|
| E1 | Started, or partly works. |
| E2 | Works in a hidden offline proof: controls reach the game and pictures are checked. |
| E3 | The owner played it in the headset and accepted it. |

The proofs are offline: identical-input runs compared with and without the input, aim measured against the game's own calibration, open frame measured by how much of the side area the game fills. None of them is a headset verdict.

## The Hub

- **One front door.** The Qt Hub (Windows) is the only way games are started. It is a desktop window and also a SteamVR dashboard panel. The old per-game `.cmd` launchers are archived; every one of their views has a Hub equivalent.
- **Library:** 143 games and 279 views across 12 systems. Each game lists the views it has, such as *Real 3D*, *Open frame*, *Inside the game*, *Theatre screen*, *With the gun*, *Pointer*, *Motion controls* and *On the monitor*. Opt-in views (roll steer, tilt steer, body motion) are never the default.
- **Systems started through launchers** (`play/<system>/launch-<system>.ps1 <key> -View <view>` with a `games.json` beside it): arcade guns and motion (MAME), Dreamcast and Atomiswave, Wii, GameCube. PS1, N64, Saturn and PS2 use their own launchers; Model 2, Model 3 and Time Crisis VR are started by the Hub directly.
- **SteamVR panel:**
  - A Steam-like grid of system tabs, scrolled a row at a time, with a game page.
  - A large on-panel **Back** button.
  - Double-press guards on Play.
  - The Hub keeps its own SteamVR manifest pointing at itself with absolute paths. SteamVR cannot follow `..` in a manifest path, which kept the Hub unregistered until 2026-10-21.
- **Input log:** `user/logs/overlay-input.log` in the live Hub folder records pointer events, the B action's state, every B press and what the page did with it, and any UI stall over 250 ms.
- **Not done:** easier system switching, and one layout shared by desktop and VR. Both need the owner's design choice.

## Controller rules (Quest through ALVR on SteamVR)

These come from the owner's headset sessions and apply to every setup:

- **Unusable buttons:** the left **Menu** button opens the SteamVR dashboard, and the right **Oculus** button belongs to the headset. No game control may depend on either.
- **B in the panel:** while the SteamVR dashboard is open, SteamVR does not deliver B to the Hub (its log shows B unbound for whole sessions). Use the on-panel Back.
- **Start, Plus, Home, Minus** (Dolphin, from 2026-10-21):
  - left-stick click is Start (GameCube) and Plus (Wii);
  - holding left Y for half a second is Home;
  - a tap of left Y is Minus.
- **Racers** steer with the stick and pedal with the triggers by default. Roll and tilt steering are opt-in views only.
- **Exit game** in SteamVR's dashboard ends the game cleanly on the hosts that support it (PS1, N64, Saturn host, Model 3).

## Systems

| System | Hub games / views | Best views | State | Open |
|---|---|---|---|---|
| PlayStation (acvr-ps1, SwanStation) | 22 / 37 | Theatre screen with the gun, diorama | E2; per-game aim trims, gun-flash hiding gated to shots | PS1 open frame not possible (games cull outside 4:3) |
| Nintendo 64 (acvr-ps1, GL interception) | 22 / 47 | Real 3D, surround view, HUD on a panel | E3 for Mario Kart 64 3D ("looks really good"), rest E2 | Start is still on the left Menu: needs a host change |
| Saturn (acvr-ps1, Beetle) | 5 / 5 | Theatre with the Stunner | E2 | Draw-order depth experiment too unreliable for use |
| PlayStation 2 (PCSX2 VR) | 12 / 27 | Screen, Inside the game | E2; Time Crisis 3 calibration and Inside fixed | |
| Sega Model 2 (MAME 0.289 patch) | 14 / 25 | Real 3D, open frame | E2; aim fixed (HotD 0.073 to 0.003 of the screen) | Sky Target firing unproven |
| Sega Model 3 (Supermodel Next10) | 11 / 54 | Open frame, Inside the game (head-tracked), two-handed wheel | E3 for Sega Rally 2 DX inside with the wheel, rest E2 | |
| Time Crisis (Namco System 22, Time Crisis VR) | 1 / 5 | Open frame, Inside | E2 | |
| Arcade guns and motion (MAME) | 26 / 34 | Theatre, open frame where the game draws it, body motion opt-in | E3 for Time Crisis theatre, rest E2 | |
| Dreamcast (Flycast hotd2-vr) | 5 / 7 | Real 3D, open frame | E2 (Confidential Mission rebuilt after E1 headset issues) | Death Crimson OX/2, Virtua Cop 2 at E1 |
| Atomiswave (Flycast) | 5 / 10 | Real 3D, open frame | E2 | |
| Wii (DolphinXR) | 19 / 26 | Real 3D with pointer or motion | E2; pointer aim within 1.3% | Owner 2026-10-21: stereo broken in a game (details pending); Dead Space and Metroid asked for the Nunchuk (MotionPlus now off); DK Country Returns stops at 1-1, Mario Strikers has no match yet |
| GameCube (DolphinXR) | 1 / 2 | Real 3D, HUD on a panel | E2 for F-Zero GX | Start moved to left-stick click on 2026-10-21 |
| Pinball (Visual Pinball X) | its own page | Desktop or headset | Scan finds tables in subfolders since 2026-10-20 | |

Each system's details, commands and proofs are in its setup README under [setups/](../../setups/).

## Where the live Hub is

The live build is a portable folder under the owner's private `.local/hub-live/<date>-<commit>/`. Launcher 0 and SteamVR's `appconfig.json` point at it, and every switch keeps backups of both. Each release folder under `.local/review/hub-release-<commit>/` holds the receipts: test log, package, owner-state staging with a settings hash, an offscreen smoke picture, and the switch record.

## What is not established

- No headset acceptance (E3) for most games: the offline proofs show controls and pictures, not comfort or feel.
- Wii stereo problem reported 2026-10-21, details pending.
- A packaged Hub has no offscreen Qt plugin. Diagnostics take it from the build Qt (see `hub/SETUP.md`).
- Windows CI's `hub-integration` crashed intermittently on a pid-file race. Fix in PR #119.
