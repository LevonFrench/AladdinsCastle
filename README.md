# AladdinsCastle

**Install and play 3D light gun and racing games in true VR. Step into the game, not in front of it.**

AladdinsCastle is an open-source desktop hub that installs and launches VR setups for arcade and console 3D light gun games (Time Crisis, Virtua Cop, House of the Dead...) and racing games (Rave Racer, Daytona USA, Scud Race...) in **true 3D VR**. The game's own world is drawn around you in stereo, the gun is your motion controller, and racing cabinets appear as **ghost controls** you grab to drive. Analog triggers act as the pedals.

> **Status: design phase.** No playable build yet. This repository holds the design docs. To play 3D light gun games in VR on a Quest 3 **today**, see [docs/quest3-quickstart.md](docs/quest3-quickstart.md).

## What it will do

- **A desktop Hub** (modelled on [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub)): a library with art, detection of your ROMs and PC games, one-click install of each game's VR setup from original sources, updates, launch, SteamVR library shortcuts, and **Install to Quest** for standalone setups. No VR lobby: games open straight into VR.
- **True 3D** wherever a stereo setup exists: our own setups (decompilations and emulators plus our shared VR runtime, libacvr), plus existing VR ports installed by recipe.
- **Theatre mode** for everything else: the emulator on a virtual screen with motion-controller aiming.
- **Motion-controller light guns** with parallax-correct aim, off-screen reload, physical ducking for cover, recoil haptics, and two-gun co-op.
- **Ghost controls for racing**: grab the wheel, shifter, handlebars or levers. Pedals on the triggers. Haptic detents and force feedback.
- **Everything is a file.** Games, VR setups, install recipes, control sets, guns and profiles are plain TOML you can edit, share and override.

## Documentation

| Doc | |
|---|---|
| [Vision](docs/vision.md) | Pillars, audience, non-goals, experience sketch |
| [Landscape](docs/landscape.md) | Existing projects we build on and learn from |
| [Architecture](docs/architecture.md) | Hub, VR setups, libacvr shared runtime, stereo techniques, input bridge |
| [Controls](docs/controls.md) | Gun aim math, cover, reload, ghost controls for racing, comfort |
| [Config spec](docs/config-spec.md) | Open-ended, layered TOML for games, setups, recipes, controls, guns |
| [Front end](docs/frontend.md) | The Hub: library, recipes, install, launch, art, technology choice |
| [Game packages](docs/game-packages.md) | One folder per game, like one mod in the Installer Hub ([examples](games/)) |
| [Game schema](docs/game-schema.md) | `game.toml` fields and filters: genre, manufacturer, year, hardware ([vocab](data/vocab/)) |
| [UI spec](docs/ui/) | Hub UI modelled closely on PCVR Mods Installer Hub: shell and filters, game cards, detail page and install flow |
| [Emulators](docs/emulators.md) | Which emulators to target first, for true 3D and for theatre |
| [Catalog](docs/catalog.md) | Light gun and racing games, with the best route for each ([full tables](docs/data/)) |
| [Roadmap](docs/roadmap.md) | Milestones M0-M8 |
| [Workshop](docs/workshop.md) | Open decisions and recommendations |
| [Quest 3 quickstart](docs/quest3-quickstart.md) | Play Time Crisis VR, Virtua Cop 2 VR and PS2 games in VR now |
| [Legal](docs/legal.md) | No ROMs, ever; licences; trademarks |

## Standing on shoulders

AladdinsCastle exists because of
[namco22-decompile](https://github.com/spacestate1/namco22-decompile),
[Time Crisis VR](https://github.com/DR-89/time-crisis-vr),
[VC2VR](https://github.com/NeuralF/Rea-Virtua-Cop-2-VR),
[PenguinScreen2](https://github.com/PenguinVRLab/PenguinScreen2),
[Supermodel](https://github.com/trzy/Supermodel),
[MAME](https://github.com/mamedev/mame),
[PCSX2](https://github.com/PCSX2/pcsx2),
[DemulShooter](https://github.com/argonlefou/DemulShooter)
and many others. See [landscape.md](docs/landscape.md).

## No game content

This project does not include, download or link to ROMs, BIOS files, disc images or game binaries. You must supply your own legally obtained copies. See [docs/legal.md](docs/legal.md).

## Licence

[GPL-3.0](LICENSE). Third-party projects keep their own licences.
