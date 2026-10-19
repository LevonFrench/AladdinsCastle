# Dreamcast and NAOMI light-gun games in VR (hotd2-vr)

This setup builds [mikermak/hotd2-vr](https://github.com/mikermak/hotd2-vr), a Flycast fork
that rebuilds Dreamcast and NAOMI games in true 3D for PC VR (OpenXR) and Quest, and adds a
small patch of our own. It ships no game files and no BIOS: the user's own disc images are
needed, and none is required for the Dreamcast BIOS (Flycast emulates it).

Upstream: `upstream-pin.json`. Licence: Flycast and hotd2-vr are GPL-2.0-or-later; our patch
is under the same terms. The arcade gun model inside hotd2-vr is CC BY 4.0 (see its header).

## What the patch adds

| Change | File | Effect |
|---|---|---|
| Death Crimson OX profile | `core/rend/vr_reproject.cpp` | Hides the game's letterbox bars (opaque black quads at W 1.6667, top and bottom, in cutscenes and play), which otherwise float in the 3D world. Game id `T2401N` (US disc). Focal length, field of view and hands are not profiled yet. |
| Pistol blowback | `core/rend/vr/xr_gun.cpp` | The House of the Dead 2 pistol's slide snaps back on every shot (25 ms back, home by 90 ms), on top of racking it by hand. |
| Arcade gun trigger | `core/rend/vr/xr_gun.cpp` | The arcade gun's trigger blade swings back up to 15 degrees with the controller's trigger. The blade was located in the mesh by position (inside the guard, on the centre line). |
| Black-bar finder | `core/rend/vr_reproject.cpp` | With `ACVR_LOG_BLACK` set, logs each W at which a game draws a flat opaque black polygon, to find a new game's letterbox W. |
| Hidden test window | `core/sdl/sdl.cpp` | With `ACVR_HIDDEN` set, the window is created hidden (unattended test runs). |
| Scripted presses | `core/rend/vr/debug_shots.cpp` | With `ACVR_PRESS="seconds:mask;..."`, presses buttons on port 1 at those game times (8 Start, 4 A, 128 right), for unattended test runs. |
| Highest refresh rate | `core/rend/vr/xr_host.cpp`, `core/cfg/option.cpp` | `vr.XrRefreshRate` 0 (the new default) asks the headset for its highest rate instead of 72 Hz. |
| Scripted light gun | `core/rend/vr/debug_shots.cpp` | `ACVR_GUN="seconds:x:y:buttons;..."` holds player 1's gun at x, y (1/10000ths of the screen, outside: off-screen) with `lightgunSet` buttons (1 trigger, 2 reload, 4 Start, 256 a shot on the 2D plane) from that game time to the next entry, sent every frame. |
| Exact-time pictures | `core/rend/vr/debug_shots.cpp` | `ACVR_SNAP="seconds;..."` writes `snap_<centiseconds>.png` at those game times. With `rend.ThreadedRendering=no` two runs with the same inputs give identical pictures (checked on The House of the Dead 2: 20 of 20). |
| End, rip, watch, poke | `core/rend/vr/debug_shots.cpp` | `ACVR_QUIT=seconds` ends the run; `ACVR_RIP="seconds:count,every,ram"` asks for a rip at a game time; `ACVR_WATCH="offset:f|h|w,..."` logs RAM values when they change; `ACVR_POKE="offset:value,..."` writes 16-bit values every frame (to find and test a game's gun calibration). |
| Wide preview | `core/rend/vr_reproject.cpp` | `ACVR_PREVIEW_WIDE=k` shows k times the stock view in the flat picture, the 2D plane still at its stock size: what the headset shows around the original frame (open-frame pictures). The shot log also gives the centre of each polygon near a shot. |

The hidden window and every `ACVR_*` test aid change nothing unless their environment variables are set.

## Build on Windows

1. Clone the upstream at the pinned commit with its submodules, then apply the patch:
   `git apply patch/acvr-hotd2-vr.patch`.
2. Build with `tools/build.cmd <source folder> <vcvars64.bat> <cmake.exe> <ninja.exe>`. The
   upstream `hotd2-vr/build-win.cmd` uses the CMake and Ninja that come with Visual Studio; if
   only an MSYS2 CMake is available, `tools/build.cmd` adds the three flags that keep it from
   picking up MSYS2's own libraries and its missing `cmcldeps` (see the comments there).
   The configure step downloads the Khronos OpenXR SDK source pinned by checksum in the
   upstream CMakeLists.

## Play

Copy `build-win\flycast.exe` to a folder of its own (its settings and saves live beside it),
add an `emu.cfg` with `vr.Xr = yes` under `[config]`, start SteamVR, then:

    flycast.exe -config "config:vr.Xr=yes" "<your disc image>"

Controls are in the upstream README (trigger shoots, shoot away from the view to reload, A Start).

## Test without a headset

`tools/Run-Capture.ps1` runs a game hidden at below-normal priority with a time limit and
saves pictures from inside the program (upstream's `vr.DebugShots`), with optional scripted
presses and a turned view (`config:vr.Yaw=25`) to see the 3D rebuild beyond the original screen.
`tools/find_bars.py` lists wide flat polygons in a rip pass (`rip.request`, see upstream).

`tools/Run-Test.ps1` is the newer runner for all the `ACVR_*` aids: `-Exe`, `-RunDir` (a scratch copy, so play
settings and saves are untouched), `-Out`, `-Game`, and `-Gun`, `-Snap`, `-Quit`, `-Seed` (the same saves every
run), `-Watch`, `-Poke`, `-Rip`, `-PreviewWide`, `-StopFile`. It never lets an OpenXR runtime load. Helpers (Python,
Pillow, numpy): `sheet.py` contact sheets, `compare.py` picture-by-picture comparison of two runs (identical or
where they differ), `aimplan.py` and `sweep.py` gun scripts over nine points or a grid, `marks.py` crops around
where the gun was sent, `gunram.py` finds where a game keeps the gun position it read (RAM dumps at known points),
`findcam.py` looks for screen and viewport matrices in a RAM dump, `openframe.py` marks the original 4:3 frame in a
wide picture and measures how much of the rest holds picture.

## Findings (2026-10-19)

- The House of the Dead 2 (US), offline: a shot changes the game (its ammo count drops, the zombie hit bleeds)
  against two control runs with the same inputs and no trigger, which are identical picture for picture. Shooting
  off the screen reloads (ammo 0 back to 6).
- Its gun aim, from the game's own memory: with the gun sent to x 100, 500 and 300 (y 100, 150, 400) the game keeps
  the raw beam position at 0x8C27B77C (x + 285, y / 2 + 20, as Flycast latches it) and its own screen position as
  floats at 0x8C27B784 / 0x8C27B788 (centred, y up): -217 / 155, 172 / 99, -44 / -153, where -220 / 140, 180 / 90,
  -20 / -160 would be exact. Its calibration (likely the seven 16-bit words from 0x8C27B792, not yet tested; perhaps from the save on the
  memory card) bends the middle of the screen: up to about 25 px off in x and 15 px in y, the edges nearly right.
  The game's own shot flash agrees (its centre lands 7 to 32 px from where the gun was sent). Fix to try: calibrate
  through the game's own gun adjustment, or set the calibration words in its profile.

## Status (2026-10-14)

- The House of the Dead 2 (US): plays in the headset with the upstream profile.
- Death Crimson OX (US): plays in 3D; black bars fixed and checked off-headset in a cutscene.
- Confidential Mission (US): boots; no profile; its memory-card menu defeats scripted presses.
- Gun animations: built, not yet seen in a headset.
- 2026-10-19: the Hub (and the private launchers) plug light guns into ports A and B for every Dreamcast game:
  the build only does that itself for games it has a profile for, so Confidential Mission had no gun in the headset.
  Not yet checked in a run (the test PC's graphics card failed during the session).
