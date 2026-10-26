# Where AladdinsCastle stands (2026-10-25, main d2ab133)

This page summarizes development progress, including dated test reports. It is
not a manifest of a downloadable release. It describes main `d2ab133`, which holds
pull requests #130 to #139 (listed below). Nothing in them adds headset (E3)
acceptance. Older checkpoint reports stay in [docs/status/](./) as history. For a
new PC, start with the [tester prerequisites](../TESTER-QUICKSTART.md).

**Read the evidence levels before the state column.** Most rows below are E2, and E2
has a poor record of predicting E3 here: of the 11 verdicts the owner gave in two headset
sessions (2026-10-19 and 2026-10-22), three were OK (each with a caveat) and eight were
wrong, broken or would not start, several after offline proof. Treat an E2 row as
unproven in the headset until a headset pass confirms it.

**Evidence levels** used below and in the setup READMEs:

| Level | Meaning |
|---|---|
| E1 | Started, or partly works. |
| E2 | Works in a hidden offline proof: controls reach the game and pictures are checked. |
| E3 | The owner played it in the headset and accepted it. |

E3 is only ever the owner's. Agents can produce E1 and E2, never E3.

The proofs are offline: identical-input runs compared with and without the input, aim measured against the game's own calibration, open frame measured by how much of the side area the game fills. None of them is a headset verdict.

## Merged since 2026-10-21 (PRs #130 to #139)

| PR | What it is | Evidence |
|---|---|---|
| #130 | Website: ASCII attract screens, page art and a music EQ (static site; not part of the 0.01 gate) | Builds in CI; not a game feature |
| #131 | Release 0.01 audit: tester guidance and licence notices | Documents |
| #132 | Hub importing is honest: a launcher game with a missing file says so, the Game library search can be stopped and reuses unchanged files, a stale Set-up report refreshes at start | CI; E2 offscreen |
| #133 | Guns R10 to R12 wired in (energy gun, heavy revolver, cutter) | Offline checks; no headset look |
| #134 | Pinball: ROM states and VPS matching, scan reruns once | CI; the owner's report of missing tables on 2026-10-22 has not been rechecked in the headset |
| #135 | Wii: bounded offline scorecards, savestate routes and a Crossbow theatre candidate | E2 only, see the Wii row |
| #136 | Hub test reliability: deterministic waits, bounded fixture cleanup | CI; see Tests below |
| #137 | The grip hand puts the index finger on every pistol's trigger | Geometry test and CPU renders; not seen in the headset |
| #138 | Disc matching: identifies discs from shared public data with cancellable readers, exact IDs for more formats | Fixture and exact-ID tests; recognised is not playable |
| #139 | 3DS (Azahar) through the PS1 host: both eyes, a Star Fox 64 3D pad map, a theatre crop | E2 offline; not launchable from the Hub, never in the headset |

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
- **Wired later:** R10 to R12 (`game-energy-gun`, `game-revolver-heavy`, `game-tool-cutter`) are wired by PR #133. R13 is not wired: `generic-pistol` (#011, the owner's rated model) stays.
- **Evidence:** offline checks and the studio review only. No headset verdict on the new shapes yet; the owner called one old gun model "god-awful" on 2026-10-19 and the replacement has not been seen. DolphinXR plays guns baked to its own `.dgv` format; those were re-baked from the new models in the owner's private play folder (the old ones kept as backups).
- Builder source: `assets/guns/src-redesign/`. The older `assets/guns/src/` still supplies the plans and metadata.
- **Grip hands (2026-10-23):** the shared hand pose had the middle finger on the trigger and the index finger lying along the frame, because the redesign moved the triggers lower and further back. `tools/hand_assembly.py` now measures each pistol's trigger blade in its GLB and moves the hand so the index fingertip rests on the front of the blade, three quarters of the way down, with the middle finger under the guard. The hands move 2.9 to 4.1 cm down and 1.1 to 2.6 cm back (`generic-pistol`: 1.4 cm forward, its trigger sits further forward). `mnt-mg-heavy` and `con-stick-grip` keep their poses. A test in `tools/test_hand_grip_coverage.py` checks, for both hands on every pistol, that the index finger is the one on the trigger; the grip review pictures were rendered again on the CPU. `generic-pistol`'s middle finger still touches the bottom of its tall guard by about 5 mm.
  - **Evidence:** offline only (geometry test and CPU renders). Not yet seen in the headset. The gun's mount on the controller did not change, so the drawn hand now sits 3 to 5 cm lower and further back on the controller than before. Check in the headset that it still lines up with your real hand.
  - The gun room reads `assets/hands/assembly.json` when it starts; nothing in this repository keeps its own copy. If the DolphinXR `.dgv` bakes in the private play folder include the hands, re-bake these: `generic-pistol`, `arc-pistol-slide`, `arc-pistol-twin`, `con-pistol-slim`, `con-pistol-dpad`, `con-revolver-chunky`, `con-remote-shell`, `game-energy-gun`, `game-revolver-heavy`, `game-tool-cutter`.

## Systems

| System | Hub games / views | Best views | State | Open |
|---|---|---|---|---|
| PlayStation (acvr-ps1: SwanStation and Beetle cores) | 22 / 37 | Theatre screen with the gun, diorama | E2; per-game aim trims, gun-flash hiding gated to shots. Graphics raised 2026-10-21 after offline before/after pictures: SwanStation games 5x, 4x MSAA, 24-bit colour, no dithering; Beetle (software) 3D games 4x and sprite games 2x, no dithering, PGXP memory-only (Beetle 4x leaves 3-7 ms per frame on one CPU thread) | PS1 open frame not possible (games cull outside 4:3) |
| Nintendo 64 (acvr-ps1, GL interception) | 22 / 47 | Real 3D, surround view, HUD on a panel | E3 for Mario Kart 64 3D, 2026-10-19: "looks really good", but the HUD was stuck to the frame; rest E2 | Start moved to the left stick click in the host defaults and the Hub text (2026-10-25), not yet seen in the headset; Mario Kart 64's HUD panel not seen since |
| Saturn (acvr-ps1, Beetle) | 5 / 5 | Theatre with the Stunner | E2 | Draw-order depth experiment too unreliable for use |
| PlayStation 2 (PCSX2 VR) | 12 / 27 | Screen, Inside the game | E2; Time Crisis 3 calibration and Inside fixed | Wipeout Pulse BROKE in the headset on 2026-10-22 ("no way to progress": non-gun PS2 games get no touch input; needs a controller layer). Time Crisis 3 Inside not seen in the headset |
| Sega Model 2 (MAME 0.289 patch) | 14 / 25 | Real 3D, open frame | E2; aim fixed (HotD 0.073 to 0.003 of the screen). Sound: every Model 2 game was silent from the Hub (`-sound none`) until 2026-10-21; now `-sound auto` to the Windows default output | Sky Target firing unproven |
| Sega Model 3 (Supermodel Next10) | 11 / 54 | Open frame, Inside the game (head-tracked), two-handed wheel | E3 for Sega Rally 2 DX inside with the wheel, 2026-10-19 ("works great"; Exit game then closed it cleanly); rest E2. Star Wars Trilogy aim was WRONG (not 1:1) and the stick and wheel models looked hollow | Star Wars Trilogy aim and stick/wheel models not rechecked |
| Time Crisis (Namco System 22, Time Crisis VR) | 1 / 5 | Open frame, Inside | E2 | |
| Arcade guns and motion (MAME) | 26 / 34 | Theatre, open frame where the game draws it, body motion opt-in | E3 for Time Crisis theatre, 2026-10-19: "plays and looks great", frame not open, gun model replaced since and unseen; rest E2 | Open frame for arcade Time Crisis needs MAME renderer work |
| Dreamcast (Flycast hotd2-vr) | 5 / 7 | Real 3D, open frame | E2 offline only. Confidential Mission was WRONG in the headset on 2026-10-19 (aim drifted, open frame "works, but not really") while it was still E1; it was rebuilt after and has not been seen since | Death Crimson OX/2, Virtua Cop 2 at E1; Confidential Mission needs a headset recheck |
| Atomiswave (Flycast) | 5 / 10 | Real 3D (the world around the frame is part of it) | E2; owner 2026-10-21: Sports Shooting USA open frame was squeezed and shifted (aim up to 270 px off), fixed in the build, offline-checked | Title and menu screens show stray art around the frame. The open frame view of Sports Shooting USA is removed because the game draws outside its frame; its aim fix is in `setups/flycast-hotd2-vr/patch/acvr-hotd2-vr.patch` |
| Wii (DolphinXR) | 20 / 28 | Qualified stereo/screen candidates; Crossbow uses mono theatre | E2 only, as of the Wii rounds merged in PR #135. 20 savestate routes restore; that proves a state loads, not that the game plays. Source windows were recorded for all 19 native and 38 forced 72/90 Hz cases, but several routes are menus, tutorials or stationary scenes, some checkpoints end within 11 to 18 s, and only about half of the timing windows were clean. Source counts do not measure headset delivery. No complete game pass, no E3 (Mario Kart Wii was WRONG in its one headset run on 2026-10-19: jaggies and the Wii Remote stand-in). Historical aim percentages and earlier gameplay proofs do not qualify every current view. Crossbow theatre shows a timed target-practice scene; its open-frame trial failed | HUD trials for Galaxy 2, Umbrella and Kirby remain rejected or inconclusive. Metroid single-core with the experimental patch OFF records 3596/3596 fresh stereo sources and zero FIFO errors in 60 wall seconds, but clipping remains; no live fix accepted. Kart 3x/4x source counts match; retain 3x/MSAA 4. Physical scale, headset pacing, and complete controls/aim acceptance remain open. [Methods and limits](../../setups/dolphin-vr/README.md#offline-measurement-review--2026-10-23) |
| GameCube (DolphinXR) | 10 / 13 | Real 3D, HUD on a panel | E2 for F-Zero GX (owner 2026-10-26: "was great"). 2026-10-26: nine more games in the launcher and catalog (Mario Kart: Double Dash!!, Kirby Air Ride, Rogue Leader, Super Mario Sunshine, Super Monkey Ball, The Wind Waker, Pikmin, Luigi's Mansion, Twilight Princess), each E2 offline: boots to scripted play, both-eye speed measured. None seen in the headset | Rogue Leader is the tightest on speed (1.9x real time at the busiest moment in the stereo proxy). Twilight Princess and Wind Waker start their save prompt on No: push the stick left, then A. The D-pad is unmapped, so Rogue Leader wingman commands are missing. Details in the gamecube README |
| Pinball (Visual Pinball X) | its own page | Desktop or headset | The scanner finds tables in subfolders. Saved scans use a fingerprint of the scanner version and table/ROM/database state; stale scans trigger a rescan. Table counts depend on the user's folders. | The owner reported on 2026-10-22 that almost all VR tables were still missing from the Hub page; later fixes (#129, #134) are not rechecked in the headset. Readability, ROM detection and launch isolation must be checked for each selected table; a scan result is not a playability verdict. |
| Nintendo 3DS (acvr-ps1 with the Azahar core) | 0 / 0 (no Hub entry) | Theatre with both eyes drawn by the game | E2 offline for Star Fox 64 3D, Kid Icarus: Uprising and Mario Kart 7: both eyes are drawn and per-depth disparity is measured (Mario Kart 7 race 1 to 28 px; Kid Icarus hit the 40 px search limit, so that figure is not a measurement). Star Fox 64 3D has a pad map; in its training course only the laser and the bomb show a visible effect between frames, the other controls were accepted without a controlled before/after. The theatre crop is proven on a boot logo and a training frame, not in the live headset loop. Never seen in a headset | Not launchable: no Hub entry, no launcher, no catalog entry. The flag flip a decrypted dump needs is not in the repository, and the core is an unsigned nightly with no pinned hash. Work continues (owner decision, 2026-10-25) |

Each system's details, commands and proofs are in its setup README under [setups/](../../setups/).

## What a tester can obtain

There is no published 0.01 release yet. CI stages a portable Windows Hub artifact
from a source revision; it does not supply the custom emulator builds, private
launchers, game files or a tested Quest connection. Every VR emulator is a patch
the user builds, and the libretro host `acvr-ps1.exe` (PS1, N64, Saturn and now 3DS)
is built by hand and is not built in CI. The install steps have not been run on a clean PC. Private development settings
and live-build locations are not installation instructions. See the
[tester walkthrough](../TESTER-QUICKSTART.md) and the selected setup's README.

## What is not established

- No headset acceptance (E3) for most games: the offline proofs show controls and pictures, not comfort or feel. The E3 results that exist: Mario Kart 64 3D (with a HUD caveat), Sega Rally 2 DX inside with the wheel, arcade Time Crisis theatre, and the SteamVR Exit game handling. Everything else, including every Wii and 3DS result, is E2 or lower.
- Wii wording: "20 of 20 games" means 20 savestate routes restore. It does not mean 20 games were played.
- Disc matching (PR #138): "recognised" counts paths, not games. A matched path can be a region or format duplicate, and a recognised disc is not a launchable one.
- 3DS: laser and bomb are the only controls with a visible before/after effect; the rest were accepted without a controlled comparison. The "realign the gyro controls" prompt stays on screen after the tutorial. Speeds above 90 Hz were measured uncapped and headless, without the OpenXR submit and the ALVR encode that share the GPU.
- CI covers the Hub, catalog schema, launch-command generation, install-engine fixtures, offscreen QML, the Python tools' own tests, the libacvr ABI and that emulator patches still apply to their pinned commits. It does not compile `acvr-ps1.exe` or any emulator, and it shows no frame reaching a headset.
- Local tests: on 2026-10-24 two Hub tests (`hub-smoke`, `hub-install`) timed out under load and the rerun did not finish, so those two failures are neither explained nor confirmed as load-only. PR #136 made the Hub test waits deterministic and bounded the fixture cleanup; a clean local rerun has not been recorded.
- Wii: current source measurements do not establish headset frame delivery. Metroid's single-core result is an isolated candidate with clipping still present; the experimental XFB patch is default-off. Full-screen-pass HUD placement, physical scale and complete controls acceptance remain open (see the Wii row).
- Open: a clean Metroid view and headset pacing; a Donkey Kong Country Returns diorama depth profile (offered, not made); a headset check of N64 Start on the left stick click (host default changed 2026-10-25); a headset look at gun models R10 to R12 and the new grip hands; easier system switching and desktop/VR parity (owner design call).
- A packaged Hub has no offscreen Qt plugin. Diagnostics take it from the build Qt (see `hub/SETUP.md`).
- Windows CI's `hub-integration` crashed intermittently on a pid-file race; the fix is in PR #119, merged.
