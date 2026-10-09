# Landscape: who is already doing what

Checked 2026-10-08. "Activity" is the repository's last push date.

## True-3D VR for arcade light gun games (closest prior art)

| Project | What it does | Licence | Activity | How we use it |
|---|---|---|---|---|
| [DR-89/time-crisis-vr](https://github.com/DR-89/time-crisis-vr) | Time Crisis in true VR on **Quest 3 standalone** and Windows PCVR: tracked pistol, stereo, physical ducking or grip cover, 120 Hz target | MIT (port code) | v0.8.4, 2026-10-07 | Hand-off backend; reference for gun projection and cover. Possible collaborator. |
| [NeuralF/Rea-Virtua-Cop-2-VR](https://github.com/NeuralF/Rea-Virtua-Cop-2-VR) (VC2VR) | Intercepts the 1997 PC Virtua Cop 2 renderer, rebuilds the 3D scene, renders through OpenXR; motion-controller gun, two-gun player 2 | MIT | 2026-09-02 | Hand-off backend; the renderer-intercept pattern for other PC ports (HotD 1-3, Virtua Cop) |
| [PenguinVRLab/PenguinScreen2](https://github.com/PenguinVRLab/PenguinScreen2) | PCSX2 fork: virtual theatre for every PS2 game, per-eye stereo and head-driven camera for profiled games, YAML profiles | GPL-3.0 (profiles: non-commercial) | v1.0-rc2, 2026-10-05 | Hand-off backend on Linux; the model for pcsx2-vr and for drop-in profile files |

## Engines with source-level access (where true 3D is possible)

| Project | Covers | Licence | Activity | Stereo seam |
|---|---|---|---|---|
| [spacestate1/namco22-decompile](https://github.com/spacestate1/namco22-decompile) | Namco System 22/21: Time Crisis, Rave Racer, Ace Driver, Dirt Dash, Prop Cycle, Tokyo Wars, Cyber Sled, Cyber Commando; Windows + Linux | MIT | 2026-10-09, very active | `engine/geo_hw.c`: hardware-accurate projection `screen = centre + X·zoom/Z` (byte-exact against MAME) |
| [trzy/Supermodel](https://github.com/trzy/Supermodel) | Sega Model 3 | GPL | 2026-09-28 | New3D renderer: projection in `CNew3D::CalcViewport`, base transform in `InitMatrixStack` |
| [lindbergh-loader/lindbergh-loader](https://github.com/lindbergh-loader/lindbergh-loader) | Runs Sega Lindbergh games (x86 Linux binaries) natively | custom | 2026-08-12 | OpenGL interposer at the loader boundary |
| [dmanlfc/sm2-emu](https://github.com/dmanlfc/sm2-emu) | Sega Model 2 emulator, open renderer (Vulkan/GLES, Linux) | custom | 2026-10-08 | Not yet read |
| [Subarasheese/daytona-xbla-recomp](https://github.com/Subarasheese/daytona-xbla-recomp) | Daytona USA (Xbox 360 release) static recompilation | not stated | 2026-07-26 | Native binary; render path hookable |
| [emoose/OutRun2006Tweaks](https://github.com/emoose/OutRun2006Tweaks) | OutRun 2006 PC wrapper DLL | GPL-3.0 | 2026-09-29 | Hooks into the game loop and renderer |
| [r4dius/outrun2-decomp](https://github.com/r4dius/outrun2-decomp) | OutRun 2 decompilation (unfinished) | not stated | 2026-10-07 | Full code access, still early |

No VR or stereo work was found for Model 2 Emulator, Flycast, Demul or TeknoParrot. The mainline Dolphin OpenXR pull requests were closed without merging. The old Dolphin VR fork is Rift-only and abandoned.

## VR light gun front ends and tools

| Project | Type | Notes |
|---|---|---|
| QuestGun | Quest app (listed 2026-08-25) | Touch controller as a light gun, user-supplied libretro cores, flat floating screen. The nearest competitor pattern. |
| [ScreenBlasterVR](https://jollykai.itch.io/screenblastervr) | Paid Windows tool | SteamVR controllers as light guns for flat MAME and TeknoParrot (cursor mode and raw HID mode) |
| EmuVR | Windows VR front end | 90+ RetroArch systems in a virtual room; user-made models; light guns through CRT emulation |
| New Retro Arcade: Neon, Lightcade VR | VR arcades | Screen-in-room. Lightcade's source is unreleased. |

## Racing in VR with motion controllers

| Project | Notes |
|---|---|
| [iChris4/Wiicompiled_VR](https://github.com/iChris4/Wiicompiled_VR) | OpenXR Mario Kart Wii fan project with grab-and-steer on Quest; a direct precedent for ghost wheels |
| VTOL VR | The reference for grabbed cockpit controls: soft lock, deliberate grip, haptic feedback |
| [mdovgialo/steam-vr-wheel](https://github.com/mdovgialo/steam-vr-wheel) | VR controllers to a vJoy wheel for PC games (MIT; inactive since 2021); reimplement the idea, don't depend on it |

## Inspired-by commercial VR games

*Under Cover* (Time Crisis-like, Quest 2024), *Crisis VRigade* (SideQuest), *Zombieland VR: Headshot Fever* (HotD-like), *On Point* (Point Blank-like). They prove the demand. They don't run the originals.

## Input plumbing

| Tool | Role | Status |
|---|---|---|
| [DemulShooter](https://github.com/argonlefou/DemulShooter) | Per-player gun input and outputs for many Model 2/3, NAOMI, Lindbergh, RingWide, TeknoParrot gun games (memory hooks) | Active (v17.9) |
| vJoy (Brunner fork 2.2.x) | Signed virtual joystick with FFB, for wheels and pedals | Maintained, low activity |
| HidHide | Hide physical controllers from games | Active |
| ViGEmBus | Virtual Xbox/DS4 pads | **Archived 2023**, don't build on it |
| MAMEHooker | Outputs (recoil, lamps) from MAME | Long-standing |

## What nobody has done yet (our opening)

1. A **front end** that ties these together: one hall, one config system, true 3D where possible and theatre elsewhere.
2. A **backend protocol** so the hall, not each port, owns hands, guns, ghost controls, comfort and UI.
3. **Motion-controlled racing** on arcade originals: ghost wheel, shifter, pedals and handlebars.
4. True 3D for **Model 3, Lindbergh and PS2 gun games on Windows**.
