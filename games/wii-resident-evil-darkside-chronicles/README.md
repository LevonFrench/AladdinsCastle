# Resident Evil: The Darkside Chronicles

The ordinary Dolphin route runs your own Wii disc on the desktop. The separate
**DolphinXR (experimental VR)** variant installs a pinned DolphinVR Redux v0.4
build and an isolated user folder for headset evaluation. It is work in progress:
our steady HUD, wide frame, in-world gun and shot-placement acceptance is open.

## Install and play

Choose your own disc with **Find my files**, select the variant, and install.
The pinned emulator and its notices stay in that game's installation. Your disc
stays where it is. No Steam shortcut or system install is part of this recipe.
Connect the Quest through ALVR and SteamVR before launching the VR variant.
It starts with one native OpenXR Wii Remote; use the fork's controller mapper
for the initial sitting. Do not enable DSU simultaneously for that same Remote.

The ordinary Dolphin stereo fallback uses the separate Wii bridge/profile setup;
this VR variant does not automatically install or launch that bridge.

## Current limits

No chapter or controller-motion acceptance has been recorded. Co-op, gun depth,
world scale, HUD framing and game-side culling remain to be measured. No 60 fps
code is enabled: verify the exact disc region and code before applying one.
Darkside camera stabilization is required as a comfort option and is not implemented yet.

Saves and settings are private under the variant's User folder. Update staging
retains runtime files, and configuration seeds apply only when each file is
absent, preserving your tuning. Existing ordinary Dolphin settings are not used.

## Credits

[Dolphin Emulator](https://github.com/dolphin-emu/dolphin) and
[DolphinXR / DolphinVR Redux](https://github.com/iChris4/dolphinXR/releases/tag/v0.4).
Included license notices are retained. No game content or Nintendo system files
are supplied by this project.
