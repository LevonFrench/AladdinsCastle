# Roadmap (draft 1)

Each milestone ends with something you can do in the headset or the Hub. Dates are not set.

## M0: Play now
- [Quest 3 quickstart](quest3-quickstart.md): DR-89 Time Crisis VR, VC2VR, PenguinScreen2.
- **Done when (optional):** the owner has played Time Crisis VR or VC2VR on PCVR (ALVR + SteamVR) and noted what feels right and wrong. Not a blocker for M1.

## M1: Hub + emulators, flat launch
- Spikes from hub-architecture.md: S1 QML → SteamVR dashboard overlay, S2 400-card grid performance.
- Desktop Hub: library grid with local/fallback art, game detail page, settings, portable folder layout.
- Config loader (layered TOML) + validation + `hubtool explain`.
- Media scanner + local art resolver (art beside the ROMs).
- Install engine core; **emulators installed from official sources or located** via `data/emulators/*.toml` (M1 set: Supermodel, MAME, PCSX2).
- **Automatic flat variants:** every game whose route lists a working emulator with a manifest gets "Play in <emulator>" (flat window), with no hand-written recipe ([game-packages.md](game-packages.md) §4.1).
- Launch + Steam library shortcuts (`--launch`).
- **Done when:** on the owner's PC, the Hub finds the owner's games and emulators, installs Supermodel from upstream, and launches **Scud Race** (Supermodel), **Time Crisis** (MAME) and **Time Crisis II** (PCSX2) in flat windows, from the Hub and from a Steam library entry. Executed by Codex from [tasks/m1/](tasks/m1/README.md). Third-party VR ports (DR-89, VC2VR) are out of scope (owner, 2026-10-08).
- **Stretch (optional):** install **hotd2-vr** from upstream for The House of the Dead 2 (Dreamcast) through its recipe (D44): the first true-3D game the Hub installs, with no new engine features.

## M2: namco22-vr: our first true-3D setups (libacvr v0 built with it)
- libacvr v0 on the [contract](libacvr-contract.md): OpenXR session, multiview, recenter/height, pause overlay, gun module (aim projection, cover button, recoil), ghost-control framework (wheel, `shifter_hl`, pedals, buttons), comfort basics.
- libacvr host on namco22-decompile (guarded build-time patches), stereo through the `geo_hw.c` projection.
- **Time Crisis** (tracked pistol, cover on the grip button) and **Rave Racer** (ghost wheel, two-position shifter, trigger pedals).
- **Control Mapping mode** ([controls-catalog.md](controls-catalog.md) §7) and the first 3D control models (wheel, H/L shifter).
- **Gun models:** libacvr loads and draws our own gun models in the hand ([guns.md](guns.md)); tier 1 models are built ahead of M2 ([tasks/m2/guns-models.md](tasks/m2/guns-models.md)). A gun in each hand follows with the first two-player setups (M3 theatre, M4 Model 3).
- Installed by the Hub as `acvr` variants; the Hub dashboard overlay from spike S1 becomes the in-VR Hub.
- Contribute generic changes upstream to namco22-decompile.
- **Done when:** both games play start to finish in true 3D at the headset's refresh rate on the owner's PC (ALVR + SteamVR), installed and launched from the Hub.

## M3: Theatre setups + input bridges
- acvr-theatre (window capture to a VR screen) with libacvr guns and ghost controls.
- Bridges: MAME Lua `field:set_value` (guns, pedals, wheels), PCSX2/DuckStation absolute pointer, RPCS3 per-player mouse, vJoy wheels and pedals, DemulShooter (consent install) for closed emulators.
- **Done when:** any catalog game with a working emulator can be played in VR on a virtual screen with motion-controller aim or ghost controls.

## M4: supermodel-vr
- Per-eye frustum in Supermodel New3D + libacvr, input fed in-process (no fork of the input system needed).
- First games: Scud Race (racing) and The Lost World or Ocean Hunter (gun).

## M5: Sprite-scaler depth
- Per-game depth profiles (game-RAM world Z first, per-asset `k` fallback) on the MAME route, per [scaler-and-2d-in-vr.md](scaler-and-2d-in-vr.md). First game: OutRun.

## M6: PS2 true 3D
- PenguinScreen2-style stereo on Windows (or upstream collaboration) + libacvr GunCon 2 aim. Targets: Time Crisis 2/3, Crisis Zone, Vampire Night, Virtua Cop Elite Edition.

## M7: Standalone (Steam Frame first, then Quest 3)
- **Steam Frame (SteamOS, ARM64):** Linux ARM64 builds of the Hub, libacvr and namco22-vr; then emulators with ARM64 builds.
- **Quest 3:** libacvr + namco22-vr for Android/OpenXR, installed over USB (`adb-install`).

Platform priority throughout: Steam Frame → Quest 3 over PC → Quest 3 native. M1-M6 target SteamVR on a PC, which already covers the Frame (streamed) and the Quest 3 over PC.

## Research track (runs alongside)
- Stereo seams for Model 2 (MAME TGP path, sm2-emu), Lindbergh (lindbergh-loader GL shim), Daytona XBLA recomp, OutRun 2006, Dolphin.
- Each one gets a feasibility note in the wiki before it becomes a milestone.
