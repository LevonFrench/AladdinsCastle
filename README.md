# AladdinsCastle

**A desktop Hub for light gun and racing games, with true 3D VR in development.**

AladdinsCastle is an open-source desktop Hub being built to install and launch VR setups for arcade and console 3D light gun games (Time Crisis, Virtua Cop, House of the Dead...) and racing games (Rave Racer, Daytona USA, Scud Race...). The goal is **true 3D VR**: the game's own world drawn around you in stereo, a tracked gun in your hand, and **ghost controls** you grab to drive, with analog triggers as pedals.

> **Status (2026-10-26): working on the owner's PC, not released.** The Qt Hub (Windows) is the single front door: 146 games and 285 views across 13 systems. Each game shows the views it has, such as *Real 3D*, *Open frame*, *Inside the game*, *Theatre screen* or *Motion controls*. The Hub is both a desktop window and a SteamVR dashboard panel. Systems: PlayStation, Nintendo 64, Saturn, PlayStation 2, Nintendo 3DS (two games so far), Sega Model 2 and Model 3, Namco System 22 (Time Crisis), MAME arcade guns and motion cabinets, Dreamcast, Atomiswave, Wii, GameCube, plus Visual Pinball. See **[where it stands](docs/status/current.md)** for each system's views, evidence level and open problems.

Most games are proven offline: controls reach the game, aim is measured against the game's own calibration, and the picture is checked. A smaller set has been accepted by the owner in the headset. An offline proof is not a headset verdict on comfort or feel. Each setup's README under [setups/](setups/) has its commands, proofs and limits. The [PCVR and standalone quickstart](docs/quest3-quickstart.md) covers separate existing projects.

## Trying a development build

There is no published 0.01 release yet. Downloading this source tree does not give
you a runnable Hub or the custom emulator builds. CI produces a Windows x64 Hub
artifact, but a green build is not a tested, complete game package.

Start with the [tester prerequisites and first-run steps](docs/TESTER-QUICKSTART.md).
You need a complete portable Hub folder, the supported build for your chosen
system, your own legally obtained game files, and a working PC-to-headset
connection through SteamVR. Most systems currently depend on separately prepared
emulator builds or launcher folders; the Hub does not create those for you.

## What it will do

- **A desktop Hub** (modelled on [PCVR Mods Installer Hub](https://github.com/Mr-Nlce/PCVR-Mods-Installer-Hub)): a library with art, detection of your ROMs and PC games, one-click install of each game's VR setup from original sources, updates, launch, SteamVR library shortcuts, and **Install to standalone headset** for standalone setups. No VR lobby: games open straight into VR.
- **True 3D** wherever a stereo setup exists: our own setups (decompilations and emulators plus our shared VR runtime, libacvr), plus existing VR ports installed by recipe.
- **Theatre mode** for everything else: the emulator on a virtual screen with motion-controller aiming.
- **Motion-controller light guns** with parallax-correct aim, off-screen reload, cover on a button, recoil haptics, and two-gun co-op.
- **The same controls in every game:** left stick click = Start, right stick click = Select or Coin, both sticks clicked together = put the screen in front of you again, both held for 3 seconds = quit and save. The menu buttons stay free for SteamVR. Written once in [docs/controls.md](docs/controls.md); not yet tried in a headset.
- **Nintendo 3DS in VR:** the game draws both eyes, and the Hub shows the top screen in stereo on a big screen. Star Fox 64 3D and Mario Kart 7 start from the Hub; the bottom screen can be a touch panel. Offline proofs only so far.
- **A guided Set-up:** the steps run in order, each system shows one next action, and the Hub only reads your folders. It lists files it did not recognise with a plain reason, groups duplicate discs, and suggests folders to try. Titles match despite accents, roman numerals and typos, but a file name never decides which disc it is.
- **Update checks and problem reports:** one button asks GitHub's public release list whether a newer emulator exists, and installs nothing. One more saves a ZIP of the logs with your folder names, account name and keys taken out, for you to look at before you send it.
- **Ghost controls for racing**: grab the wheel, shifter, handlebars or levers. Pedals on the triggers. Haptic detents and force feedback.
- **Everything is a file.** Games, VR setups, install recipes, control sets, guns and profiles are plain TOML you can edit, share and override.

## Documentation

| Doc | |
|---|---|
| **[Where it stands](docs/status/current.md)** | **Current state of every system, the Hub and the controller rules; start here** |
| [Hub set-up and use](hub/SETUP.md) | First run, the VR games tab, SteamVR panel, watchdog, what the Hub runs and keeps |
| [Setups](setups/) | One README per VR setup (acvr-ps1, PCSX2, Model 2, Model 3, MAME guns, Flycast, Dolphin): commands, views, proofs |
| [Vision](docs/vision.md) | Pillars, audience, non-goals, experience sketch |
| [Landscape](docs/landscape.md) | Existing projects we build on and learn from |
| [Architecture](docs/architecture.md) | Hub, VR setups, libacvr shared runtime, stereo techniques, input bridge |
| [Controls](docs/controls.md) | The universal stick-click controls, gun aim math, cover, reload, ghost controls for racing, comfort |
| [3D mode target](docs/3d-mode-target.md) | What true 3D must look like: HUD on a steady panel, a wide frame, the gun in the world |
| [Guns](docs/guns.md) | Planned gun library, two-player policy and game-to-model assignments |
| [Config spec](docs/config-spec.md) | Open-ended, layered TOML for games, setups, recipes, controls, guns |
| [Front end](docs/frontend.md) | The Hub: library, recipes, install, launch, art, technology choice |
| [Game packages](docs/game-packages.md) | One folder per game, like one mod in the Installer Hub ([examples](games/)) |
| [Game schema](docs/game-schema.md) | `game.toml` fields and filters: genre, manufacturer, year, hardware ([vocab](data/vocab/)) |
| [UI spec](docs/ui/) | Hub UI modelled closely on PCVR Mods Installer Hub: shell and filters, game cards, detail page and install flow |
| [Emulators](docs/emulators.md) | Which emulators to target first, for true 3D and for theatre |
| [Scaler and 2D games in VR](docs/scaler-and-2d-in-vr.md) | What M2 did in the Sega 3D Classics, and our layered depth method for sprite-scaler and 2D games |
| [Catalog](docs/catalog.md) | Light gun and racing games, with the best route for each ([full tables](docs/data/)) |
| [Hub architecture](docs/hub-architecture.md) | Qt 6 / QML Hub: modules, desktop + SteamVR overlay from one UI, packaging, licensing |
| [QML components](docs/ui/04-qml-components.md) · [models](docs/ui/05-models.md) · [theme](docs/ui/theme.toml) | Component map, C++→QML models and design tokens for the Mod Hub-style UI |
| [Hub in VR: menu controls](docs/ui/06-vr-menu-controls.md) | Laser, stick scrolling and keyboard rules for the dashboard overlay, with the fixes the first headset session called for |
| [Art pipeline](docs/art-pipeline.md) | Asset sizes, user-side scraping, provenance, generated fallback, source licences |
| [Install engine](docs/install-engine.md) | Recipe execution: states, step kinds, download safety, journal, uninstall, updates |
| [Emulator manifests](docs/emulator-manifests.md) | Schema for `data/emulators/*.toml`, platform matrix (incl. ARM64), licence gates |
| [Launch and runtime](docs/launch-and-runtime.md) | Pre-flight checks, runtime pinning, Steam/SteamVR library shortcuts, Steam Frame |
| [Controls catalog](docs/controls-catalog.md) | Every cabinet control (bikes, skis, horse, paddle, planes, tanks, special guns), VR mappings, 3D model library, Control Mapping mode |
| [Scaler depth probe](docs/scaler-depth-probe.md) | MAME Lua measurement of OutRun sprite scale vs road depth |
| [Roadmap](docs/roadmap.md) | Milestones M0-M8 |
| [Workshop](docs/workshop.md) | Open decisions and recommendations |
| [PCVR and standalone quickstart](docs/quest3-quickstart.md) | Play Time Crisis VR, Virtua Cop 2 VR and PS2 games in VR now |
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

Project code defaults to [GPL-3.0-only](LICENSE). **`libacvr/` has its own
[MIT licence](libacvr/LICENSE)**; upstream-derived files and emulator patches keep
their applicable notices. Original built gun, hand and control assets are CC0;
`arc-light-gun` and its previews are CC BY 4.0. See the
[licence and attribution index](THIRD-PARTY-NOTICES.md) before redistributing
source, models, a website copy or a binary package.
