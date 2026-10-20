# Playing Sega Saturn gun games from Hub

Open **VR games**. The Sega Saturn shelf lists the five light-gun games that
`acvr-ps1` (`setups/retro-ps1-vr`, `--gun stunner`) runs through the Beetle
Saturn core on a big theatre screen in the headset, with your right controller
as the Saturn Stunner (Virtua Gun): Virtua Cop, Virtua Cop 2, The House of the
Dead, Area 51 and Mechanical Violator Hakaider. The Saturn hands its graphics
chip flat 2D shapes, so these games have no real 3D view.

Hub ships no emulator, core, BIOS or game. You keep a small launcher script of
your own (Windows PowerShell, `launch-saturn.ps1`) that knows, per key, your
disc, the game's aim trim and flash setting, and a first save or start state
with the game's own gun calibration done. A `games.json` beside the script
lists the keys; they are the ids in `src/core/saturn/Saturn.cpp`
(`virtua-cop`, `virtua-cop-2`, `house-of-the-dead`, `area-51`, `hakaider`).

## Set up, once

In **Set-up**, choose the launcher script for Sega Saturn (setting
`saturnLauncher`). A card says **Not ready** when the script is missing or has
no entry for that game.

## Start and stop

Start SteamVR with the headset connected first; Hub refuses to start the game
otherwise. Hub runs Windows PowerShell 5.1 from the system folder with
`-NoProfile -ExecutionPolicy Bypass -File <launcher> <key>`, with every
inherited `ACVR_*` variable removed and `ACVR_PS1_STOP_FILE` set to a stop file
of its own for that launch, under `user/runtime/`. **Exit game** creates that
file: the game saves and quits, and Hub waits for it. Holding left Y for 3
seconds in the headset does the same.
