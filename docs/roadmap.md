# Roadmap (draft 0)

Each milestone ends with something you can do in the headset. Dates are not set.

## M0: Play now
- [Quest 3 quickstart](quest3-quickstart.md): DR-89 Time Crisis VR, VC2VR, PenguinScreen2.
- **Done when:** the owner has played Time Crisis VR on Quest 3 and written down what feels right and wrong. That feeds the controls spec.

## M1: Hall skeleton + hand-off
- Godot 4 project with an OpenXR hall: one room, generic cabinets, laser interaction, list mode.
- Config loader: layered TOML, JSON Schemas, a `validate` and `explain` CLI.
- Hand-off backend kind: launch DR-89 Time Crisis VR (Windows) and VC2VR, then return to the hall on exit.
- **Done when:** in the headset, you walk to a Time Crisis cabinet, pull the trigger, play, quit, and you're back in the hall.

## M2: ACBP v0 + compositor + ghost controls
- ACBP spec v0, a C header, and a test backend that renders a stereo scene with depth and a fake game camera.
- Hall compositor: backend eyes and depth composited with hands, gun and ghost controls. Display-list replay at headset rate.
- Ghost-control framework with `wheel`, `shifter_hl`, `pedal` and `button`, sending to the test backend and to vJoy.
- **Done when:** you can grab the ghost wheel and drive the test scene smoothly at 90 Hz or more.

## M3: namco22-vr, the first true-3D games through our own stack
- ACBP host on namco22-decompile, with stereo through the `geo_hw.c` projection.
- **Rave Racer** with ghost wheel, two-position shifter and trigger pedals: the first racing slice.
- **Time Crisis** through ACBP, with gun mapping from the game camera and physical ducking.
- Contribute generic changes upstream where welcome. Coordinate with DR-89, whose Time Crisis port is built on the same base.
- **Done when:** both games play start to finish in the hall at the headset's refresh rate.

## M4: Theatre mode + input bridges
- Window capture to the cabinet screen and a big screen.
- Bridges: MAME Lua light gun plugin, PCSX2 and DuckStation absolute pointer, RPCS3 per-player mouse, vJoy wheels and pedals, a DemulShooter integration for Model 2, Lindbergh and TeknoParrot gun games.
- **Done when:** any game in the catalog can be launched from the hall and played with motion controllers on a virtual screen.

## M5: supermodel-vr
- Per-eye frustum in Supermodel New3D plus ACBP output.
- First games: Scud Race (racing) and a Model 3 gun game.

## M6: Art, packs and community
- Scraper integration (user-side), art provenance sidecars, branded cabinet pack format, pack validator (rejects ROM content).
- Example packs: Namco System 22, Sega Model 3.

## M7: PS2 true 3D
- Port PenguinScreen2's stereo approach to Windows, or work with that project upstream. Add GunCon 2 aim from the VR ray.
- Targets: Time Crisis 2/3, Crisis Zone, Vampire Night, Virtua Cop Elite Edition.

## M8: Quest standalone
- Godot Android export of the hall plus namco22-vr built for Android, so System 22 games run on Quest 3 without a PC.

## Research track (runs alongside)
- Stereo seams for Model 2 (MAME TGP path, sm2-emu), Lindbergh (lindbergh-loader GL shim), Daytona XBLA recomp, OutRun 2006, Dolphin.
- Each gets a feasibility note in the wiki before it becomes a milestone.
