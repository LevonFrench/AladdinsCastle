# Roadmap (draft 1)

Each milestone ends with something you can do in the headset or the Hub. Dates are not set.

## M0: Play now
- [Quest 3 quickstart](quest3-quickstart.md): DR-89 Time Crisis VR, VC2VR, PenguinScreen2.
- **Done when:** the owner has played Time Crisis VR on Quest 3 and written down what feels right and wrong. That feeds the controls spec.

## M1: Hub skeleton + third-party setups
- Desktop Hub: library grid with fallback art, game detail page, settings, portable folder layout.
- Config loader (layered TOML) + JSON Schemas + `validate` / `explain` CLI.
- Recipe engine: `github-release`, `require-media`, `extract`, `write-config`, `shortcut`, `adb-install`.
- First recipes: DR-89 Time Crisis VR (PCVR + **Install to Quest**), VC2VR (detect the PC game).
- Media scanner: hash-check ROM folders.
- SteamVR library shortcuts with art.
- **Done when:** from a clean PC, the Hub installs Time Crisis VR (PC and Quest) and VC2VR, and both launch from the Hub and from the SteamVR library.

## M2: libacvr v0 + test setup
- libacvr: OpenXR session, multiview, recenter/height, pause overlay, gun module, ghost-control framework (`wheel`, `shifter_hl`, `pedal`, `button`), comfort basics.
- A test setup ("stereo cube") implementing the backend contract with a fake game camera.
- **Done when:** in the headset you can shoot targets with correct parallax aim, and grab the ghost wheel to drive the test scene at 90 Hz or more.

## M3: namco22-vr, the first true-3D games of our own
- libacvr host on namco22-decompile (guarded build-time patches, like DR-89's), stereo through the `geo_hw.c` projection.
- **Rave Racer** with ghost wheel, two-position shifter and trigger pedals: the first racing setup.
- **Time Crisis** on our host, with libacvr gun and cover, alongside DR-89's.
- Contribute generic changes upstream, and coordinate with DR-89.
- **Done when:** both play start to finish at the headset's refresh rate, installed by recipe from the Hub.

## M4: Theatre setups + input bridges
- acvr-theatre (window capture to a VR screen).
- Bridges: MAME Lua light gun plugin, PCSX2/DuckStation absolute pointer, RPCS3 per-player mouse, vJoy wheels and pedals, DemulShooter for Model 2 / Lindbergh / TeknoParrot gun games.
- Recipes for the emulators (download from official sources only).
- **Done when:** any game in the catalog can be installed and played with motion controllers on a virtual screen.

## M5: supermodel-vr
- Per-eye frustum in Supermodel New3D + libacvr.
- First games: Scud Race (racing) and a Model 3 gun game.

## M6: Art, recipes and community
- User-side scraper integration with provenance sidecars.
- Recipe and pack sharing; a pack validator that rejects ROM content.

## M7: PS2 true 3D
- Port PenguinScreen2's stereo approach to Windows, or work with that project upstream, adding libacvr gun aim.
- Targets: Time Crisis 2/3, Crisis Zone, Vampire Night, Virtua Cop Elite Edition.

## M8: Standalone setups (Steam Frame first, then Quest 3)
- **Steam Frame (SteamOS, ARM64):** Linux ARM64 builds of the Hub, libacvr and namco22-vr; then the emulators that have ARM64 builds. Target audience #1 (owner, 2026-10-08).
- **Quest 3:** libacvr + namco22-vr built for Android/OpenXR, installed by the Hub over USB.

Platform priority throughout: Steam Frame → Quest 3 over PC → Quest 3 native. M1-M7 target SteamVR on a PC, which already covers the Frame (streamed) and the Quest 3 over PC.

## Research track (runs alongside)
- Stereo seams for Model 2 (MAME TGP path, sm2-emu), Lindbergh (lindbergh-loader GL shim), Daytona XBLA recomp, OutRun 2006, Dolphin.
- Each one gets a feasibility note in the wiki before it becomes a milestone.
