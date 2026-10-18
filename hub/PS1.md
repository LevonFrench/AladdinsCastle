# Playing PlayStation light-gun games from Hub

Open **VR games**. The PlayStation shelf lists the light-gun games that
`acvr-ps1` (`setups/retro-ps1-vr`) shows on a theatre screen in the headset,
with your right controller as the GunCon (or the Justifier, or an arcade gun
for arcade sets played through the MAME core).

Hub ships no emulator, BIOS or game. You keep a small launcher script of your
own (Windows PowerShell) that knows, per game key, your disc, which gun the
game reads, and which core to run. A `games.json` beside the script lists the
keys; the keys are the ones in `src/core/ps1/Ps1.cpp` (`point-blank`,
`time-crisis`, `arcade-point-blank-2` and so on).

## Set up, once

In **Set-up**, choose the launcher script for Sony PlayStation (setting
`ps1Launcher`, saved in Hub's own `user/hub-settings.toml`). A card says
**Not ready** when the script is missing or has no entry for that game.

## Start and stop

Start SteamVR with the headset connected first; Hub refuses to start the game
otherwise. Hub runs Windows PowerShell 5.1 from the system folder with
`-NoProfile -ExecutionPolicy Bypass -File <launcher> <key>`, with every
inherited `ACVR_*` variable removed and `ACVR_PS1_STOP_FILE` set to a stop file
of its own for that launch, under `user/runtime/`.

**Exit game** creates that file: `acvr-ps1` saves the memory card and quits,
and Hub waits for it. Holding left Y for 3 seconds in the headset does the
same.
