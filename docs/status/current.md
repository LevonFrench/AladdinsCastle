# Where AladdinsCastle stands (2026-10-21, evening)

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
- **Tests:** the integration tests used to copy the test program under emulator names and start the copies. Windows Defender flagged one as a trojan (a false positive) and deleted the test program mid-run. A small stand-in game (`hub/tests/StandIn.cpp`) now plays every synthetic game instead (PR #123).
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

## Guns and controller models

- **Redesign (2026-10-21, PR #123):** the owner marked 50 earlier gun and controller models for replacement. Nine are now new original designs (review IDs R01 to R09): `arc-pistol-slide`, `arc-pistol-twin`, `con-pistol-dpad`, `con-pistol-slim`, `con-pointer-remote`, `con-remote-shell`, `con-revolver-chunky`, `con-stick-grip` and `mnt-mg-heavy`. Hosts load them unchanged because node names, anchors and motion nodes keep the conventions in [assets/guns/README.md](../../assets/guns/README.md). A four-specialist studio review (hard-surface, retopology, rigging and host fit, materials) corrected sizes (the Wii Remote is 148 mm), materials, sight lines and normals; `tools/check_gun_assets.py` reports 0 errors.
- **Data:** the dark accent tints of seven guns (the six redesigned ones with tints, plus `generic-pistol`, whose shape is unchanged) were lifted to `#50535a` so grips no longer read as holes under the hosts' lighting; button press axes were corrected for the D-pad pistol and the chunky revolver. The System 22 view catalog was regenerated for the new tints.
- **Not wired:** R10 to R12 are deferred. R13 is not wired: `generic-pistol` (#011, the owner's rated model) stays.
- **Evidence:** offline checks and the studio review only. No headset verdict on the new shapes yet. DolphinXR plays guns baked to its own `.dgv` format; those were re-baked from the new models in the owner's private play folder (the old ones kept as backups).
- Builder source: `assets/guns/src-redesign/`. The older `assets/guns/src/` still supplies the plans and metadata.

## Systems

| System | Hub games / views | Best views | State | Open |
|---|---|---|---|---|
| PlayStation (acvr-ps1: SwanStation and Beetle cores) | 22 / 37 | Theatre screen with the gun, diorama | E2; per-game aim trims, gun-flash hiding gated to shots. Graphics raised 2026-10-21 after offline before/after pictures: SwanStation games 5x, 4x MSAA, 24-bit colour, no dithering; Beetle (software) 3D games 4x and sprite games 2x, no dithering, PGXP memory-only (Beetle 4x leaves 3-7 ms per frame on one CPU thread) | PS1 open frame not possible (games cull outside 4:3) |
| Nintendo 64 (acvr-ps1, GL interception) | 22 / 47 | Real 3D, surround view, HUD on a panel | E3 for Mario Kart 64 3D ("looks really good"), rest E2 | Start is still on the left Menu: needs a host change |
| Saturn (acvr-ps1, Beetle) | 5 / 5 | Theatre with the Stunner | E2 | Draw-order depth experiment too unreliable for use |
| PlayStation 2 (PCSX2 VR) | 12 / 27 | Screen, Inside the game | E2; Time Crisis 3 calibration and Inside fixed | |
| Sega Model 2 (MAME 0.289 patch) | 14 / 25 | Real 3D, open frame | E2; aim fixed (HotD 0.073 to 0.003 of the screen). Sound: every Model 2 game was silent from the Hub (`-sound none`) until 2026-10-21; now `-sound auto` to the Windows default output | Sky Target firing unproven |
| Sega Model 3 (Supermodel Next10) | 11 / 54 | Open frame, Inside the game (head-tracked), two-handed wheel | E3 for Sega Rally 2 DX inside with the wheel, rest E2 | |
| Time Crisis (Namco System 22, Time Crisis VR) | 1 / 5 | Open frame, Inside | E2 | |
| Arcade guns and motion (MAME) | 26 / 34 | Theatre, open frame where the game draws it, body motion opt-in | E3 for Time Crisis theatre, rest E2 | |
| Dreamcast (Flycast hotd2-vr) | 5 / 7 | Real 3D, open frame | E2 (Confidential Mission rebuilt after E1 headset issues) | Death Crimson OX/2, Virtua Cop 2 at E1 |
| Atomiswave (Flycast) | 5 / 10 | Real 3D (the world around the frame is part of it) | E2; owner 2026-10-21: Sports Shooting USA open frame was squeezed and shifted (aim up to 270 px off), fixed in the build, offline-checked | Title and menu screens show stray art around the frame. The open frame view of Sports Shooting USA is removed because the game draws outside its frame; its aim fix is in `setups/flycast-hotd2-vr/patch/acvr-hotd2-vr.patch` |
| Wii (DolphinXR) | 19 / 26 | Real 3D with pointer or motion | E2; pointer aim within 1.3%. 2026-10-21: the "broken stereo" was each game's `[VR] VirtualScreen = False`, which glues all 2D (menus, strap screens, HUD) to the face in both eyes; Dead Space, Overkill, Gunblade, New Super Mario Bros. Wii and Punch-Out!! now put it on a screen in the room (0 to 15-41 px disparity, offline probe). The Nunchuk reaches every game (Metroid's 3-second "Nunchuk is required" is a harmless menu notice). Metroid Prime, DK Country Returns (grip = shake) and Mario Strikers Charged reach gameplay offline | Metroid Prime Trilogy drops about 60% of frames from the headset (RAM frame copies are rebuilt as one layer; needs an emulator change). Games whose picture is built from full-screen 2D passes (Galaxy, Kirby, Resort, Zelda, Strikers, Umbrella, Darkside, Sin & Punishment) keep 2D on the face until per-game HUD shader overrides are made in a headset session |
| GameCube (DolphinXR) | 1 / 2 | Real 3D, HUD on a panel | E2 for F-Zero GX | Start moved to left-stick click on 2026-10-21 |
| Pinball (Visual Pinball X) | its own page | Desktop or headset | 597 tables, 561 startable (2026-10-21 rescan; the saved scan was from 2026-10-15). Scan finds tables in subfolders. A saved scan carries a fingerprint (scanner version 2, table count, newest table and ROM dates, database date); when it no longer matches, the Hub rescans by itself, which fixed missing tables | The Matrix and Total Nuclear Annihilation files cannot be parsed |

Each system's details, commands and proofs are in its setup README under [setups/](../../setups/).

## Where the live Hub is

The live build is a portable folder under the owner's private `.local/hub-live/<date>-<commit>/`. Launcher 0 and SteamVR's `appconfig.json` point at it, and every switch keeps backups of both. On 2026-10-21 evening the live build is the one from commit `27a0333`, which predates the Pinball rescan and the Model 2 sound fix (both are in main but not yet in a live build). A build from the PR #123 head (`a7ffcfb`) is staged beside it and is switched live after PR #123 merges. Each release folder under `.local/review/hub-release-<commit>/` holds the receipts: test log, package, owner-state staging with a settings hash, an offscreen smoke picture, and the switch record.

## What is not established

- No headset acceptance (E3) for most games: the offline proofs show controls and pictures, not comfort or feel.
- Wii: Metroid Prime Trilogy frame drops; 2D on the face in the full-screen-pass games (see the Wii row).
- Open: the Metroid frame drops (needs an emulator change); a Donkey Kong Country Returns diorama depth profile (offered, not made); N64 Start off the left Menu (host change); gun models R10 to R12; easier system switching and desktop/VR parity (owner design call).
- A packaged Hub has no offscreen Qt plugin. Diagnostics take it from the build Qt (see `hub/SETUP.md`).
- Windows CI's `hub-integration` crashed intermittently on a pid-file race. Fix in PR #119.
