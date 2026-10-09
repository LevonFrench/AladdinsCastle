# Vision

AladdinsCastle is a VR arcade for 3D light gun and racing games. You stand in an arcade hall, walk up to a cabinet, and step *into* the game: the original 3D world drawn in stereo around you, with the gun in your hand or the wheel under your hands.

It is a front end, a launcher and a set of VR backends. It does not include any games. You bring your own legally obtained dumps and PC releases.

## Pillars

1. **Into the game, not in front of it.** The default experience is true 3D: the game's own geometry in stereo with head tracking. A virtual screen is a fallback, used only when no 3D backend exists for a game yet.
2. **Your hands are the cabinet.** Motion controllers become the cabinet's controls. Guns aim where the controller points. Racing cabinets appear as see-through *ghost controls* (wheel, shifter, handlebars, buttons) that you grab to take over. Analog triggers act as pedals.
3. **The arcade is the menu.** Browsing is walking through a hall of cabinets with real art: marquees, side art, bezels, flyers and attract mode. A flat list is always one button away.
4. **Everything is a file.** Games, backends, cabinets, control sets, input bindings, themes and halls are plain-text files you can read, copy, share and override. No hidden database, no hardcoded game list.
5. **Built on the people already doing this.** Decompilation projects, emulators and existing VR mods do the hard work. AladdinsCastle orchestrates them, adds missing pieces through clean interfaces, and credits them.

## Who it is for

- VR owners (Quest 3 first, via PCVR) who grew up on Time Crisis, House of the Dead, Virtua Cop, Daytona and Ridge Racer.
- Arcade preservation and emulation hobbyists who already own dumps and want the best way to play them.
- Modders who want to add a game, a cabinet or a control set without touching code.

## What success looks like

- Version 1: a playable hall on Quest 3 over PCVR that launches at least one gun game and one racing game in true 3D. All controls work through motion controllers, and every game, cabinet and binding is defined in config files.
- Long term: most of the notable 3D light gun and racing catalog is playable in true 3D, community packs add games and cabinets, and a standalone Quest build covers the titles that can run natively on Quest.

## Non-goals

- Distributing ROMs, BIOS files, ISOs or other copyrighted game content, or linking to sources for them.
- Being an emulator. Emulation stays in the upstream projects. We add VR layers, ideally upstreamable.
- 2D sprite light gun games. They are welcome in theatre mode, but the project focuses on 3D games.
- Competitive online infrastructure, in version 1.

## Experience sketch

1. Put on the Quest 3, launch AladdinsCastle from SteamVR or through the OpenXR runtime directly.
2. You're in the hall: neon, carpet, the sound of attract modes. Cabinets are grouped by row: Gun, Racing, Namco, Sega, and so on.
3. Walk (or teleport) to *Time Crisis*. The marquee glows and attract mode plays on its screen. Pick up the gun on the holster.
4. Pull the trigger at the screen. The screen grows around you and you are standing in the castle courtyard, gun in hand. Duck for real to take cover.
5. Game over: the world folds back into the cabinet screen and your score is on the hall high-score board.
6. Next: *Rave Racer*. Sit down. A ghost steering wheel and gear shifter float in front of you. Grab the wheel, squeeze the right trigger to accelerate.
