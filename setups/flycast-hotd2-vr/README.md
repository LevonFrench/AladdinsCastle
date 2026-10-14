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

The hidden window and scripted presses change nothing unless their environment variables are set.

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

## Status (2026-10-14)

- The House of the Dead 2 (US): plays in the headset with the upstream profile.
- Death Crimson OX (US): plays in 3D; black bars fixed and checked off-headset in a cutscene.
- Confidential Mission (US): boots; no profile; its memory-card menu defeats scripted presses.
- Gun animations: built, not yet seen in a headset.
