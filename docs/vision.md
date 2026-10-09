# Vision

AladdinsCastle puts 3D arcade light gun and racing games into true VR. You pick a game in a desktop hub with proper art, press Play, put on the headset, and you are *inside* the game: the original 3D world drawn in stereo around you, the gun in your hand or the wheel under your hands.

It's an installer, launcher and set of VR setups, modelled on [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub), except it installs **our own setups** for each game. It does not include any games. You bring your own legally obtained dumps and PC releases.

## Pillars

1. **Into the game, not in front of it.** The default is true 3D: the game's own geometry in stereo with head tracking. A virtual screen is a fallback, used only when no 3D setup exists for a game yet.
2. **Your hands are the cabinet.** Motion controllers become the cabinet's controls. Guns aim where the controller points. Racing cabinets appear as see-through *ghost controls* (wheel, shifter, handlebars, buttons) that you grab to take over. Analog triggers act as pedals.
3. **One click from owning a game to playing it in VR.** The Hub finds your files, installs the right setup from original sources, configures it, and keeps it updated.
4. **Every game feels the same.** Shared VR runtime (libacvr): same gun handling, cover, ghost controls, comfort options and pause menu everywhere.
5. **Everything is a file.** Games, recipes, control sets, guns, bindings and profiles are plain-text files you can read, copy, share and override.
6. **Built on the people already doing this.** Decompilations, emulators and existing VR mods do the hard work. We install them, add the missing pieces through clean interfaces, contribute upstream, and credit them.

## Who it's for

- VR owners (Quest 3 first, via PCVR) who grew up on Time Crisis, House of the Dead, Virtua Cop, Daytona and Ridge Racer.
- Arcade preservation and emulation hobbyists who already own dumps.
- Modders who want to add a game, recipe or control set without touching code.

## What success looks like

- **Version 1:** a Windows Hub that installs and launches at least one gun game and one racing game in true 3D on a Quest 3 over PCVR. All controls go through the motion controllers, and every game is defined by config and recipe files.
- **Long term:** most of the notable 3D light gun and racing catalog is playable in true 3D. Community recipes add games. Quest-standalone setups cover what can run on the headset itself.

## Non-goals

- Shipping, downloading or linking to ROMs, BIOS files, disc images or other copyrighted game content.
- A VR lobby or arcade room. Games launch straight into VR.
- Being an emulator. Emulation stays upstream; we add VR layers, ideally upstreamable.
- 2D sprite light gun games, beyond theatre mode.

## Experience sketch

1. Open AladdinsCastle on the PC. The library shows marquees and flyers. *Time Crisis* says **Ready · True 3D**. *Rave Racer* says **Needs your files**.
2. Point the Hub at your ROM folder. It verifies `raverace.zip` by hash, and Rave Racer turns **Ready**.
3. Press **Play** on Time Crisis and put on the Quest 3. You are standing in the castle courtyard with a GunCon in your hand. Duck for real to take cover.
4. Hold the menu button: the pause overlay offers recenter, laser, cover mode, quit.
5. Quit and you're back on the desktop, or in the SteamVR library where every installed game also appears with its art. Pick *Rave Racer* from there without taking the headset off.
6. In Rave Racer a ghost steering wheel and gear lever float in front of you. Grab the wheel and squeeze the right trigger to accelerate.
