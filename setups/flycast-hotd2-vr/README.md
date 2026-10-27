# Dreamcast and NAOMI light-gun games in VR (hotd2-vr)

This setup builds [mikermak/hotd2-vr](https://github.com/mikermak/hotd2-vr), a Flycast fork
that rebuilds Dreamcast and NAOMI games in true 3D for PC VR (OpenXR) and Quest, and adds a
small patch of our own. It ships no game files and no BIOS: the user's own disc images are
needed, and none is required for the Dreamcast BIOS (Flycast emulates it).

Discs play in place, from the folder they are in. The Hub and the House of the Dead 2 recipe pass
`config:UseReios=yes`, so Flycast uses its built-in BIOS even when a real `dc_bios.bin` and a blank
`dc_flash.bin` sit beside the disc. Without it Flycast finds those files in the disc's folder and
runs the console's first-start Set Date/Time screen (seen with Death Crimson 2 and Virtua Cop 2).
Flycast still switches to the real BIOS by itself for the few games that need one.

Upstream: `upstream-pin.json`. Licence: Flycast and hotd2-vr are GPL-2.0-or-later; our patch
is under the same terms. The arcade gun model inside hotd2-vr is CC BY 4.0 (see its header).

## What the patch adds

| Change | File | Effect |
|---|---|---|
| Death Crimson OX profile | `core/rend/vr_reproject.cpp` | Hides the game's letterbox bars (opaque black quads at W 1.6667, top and bottom, in cutscenes and play), which otherwise float in the 3D world. Game id `T2401N` (US disc). Focal length, field of view and hands are not profiled yet. |
| Pistol blowback | `core/rend/vr/xr_gun.cpp` | The House of the Dead 2 pistol's slide snaps back on every shot (25 ms back, home by 90 ms), on top of racking it by hand. |
| Arcade gun trigger | `core/rend/vr/xr_gun.cpp` | The arcade gun's trigger blade swings back up to 15 degrees with the controller's trigger. The blade was located in the mesh by position (inside the guard, on the centre line). |
| Black-bar finder | `core/rend/vr_reproject.cpp` | With `ACVR_LOG_BLACK` set, logs each W at which a game draws a flat opaque black polygon, to find a new game's letterbox W. |
| No lettering on the arcade gun | `core/rend/vr/gun_model.h` | The maker's name engraved on the right side of the arcade gun is removed and the side refilled. The header is rewritten by `tools/clean_gun_model.py` (step 2 of the build), not carried in the patch. |
| Hidden test window | `core/sdl/sdl.cpp` | With `ACVR_HIDDEN` set, the window is created hidden (unattended test runs). |
| Scripted presses | `core/rend/vr/debug_shots.cpp` | With `ACVR_PRESS="seconds:mask;..."`, presses buttons on port 1 at those game times (8 Start, 4 A, 128 right), for unattended test runs. |
| Highest refresh rate | `core/rend/vr/xr_host.cpp`, `core/cfg/option.cpp` | `vr.XrRefreshRate` 0 (the new default) asks the headset for its highest rate instead of 72 Hz. |
| Scripted light gun | `core/rend/vr/debug_shots.cpp` | `ACVR_GUN="seconds:x:y:buttons;..."` (and `ACVR_GUN2` for player 2) holds player 1's gun at x, y (1/10000ths of the screen, outside: off-screen) with `lightgunSet` buttons (1 trigger, 2 reload, 4 Start, 256 a shot on the 2D plane) from that game time to the next entry, sent every frame. |
| Exact-time pictures | `core/rend/vr/debug_shots.cpp` | `ACVR_SNAP="seconds;..."` writes `snap_<centiseconds>.png` at those game times. With `rend.ThreadedRendering=no` two runs with the same inputs give identical pictures (checked on The House of the Dead 2: 20 of 20). |
| End, rip, watch, poke | `core/rend/vr/debug_shots.cpp` | `ACVR_QUIT=seconds` ends the run; `ACVR_RIP="seconds:count,every,ram"` asks for a rip at a game time; `ACVR_WATCH="offset:f|h|w,..."` logs RAM values when they change; `ACVR_POKE="offset:value,..."` writes 16-bit values every frame (to find and test a game's gun calibration). |
| HOTD2 (US) gun calibration | `core/rend/vr_reproject.cpp` | The profile writes the game's seven calibration words so its aim matches Flycast's light gun exactly (see Findings). |
| Per-game aim trim | `core/rend/vr_reproject.cpp`, `core/input/udp_lightgun.cpp` | A profile can move the light gun's beam to where a game with a fixed beam mapping of its own reads the intended pixel (`GameProfile::aim`). |
| Ranger Mission profile | `core/rend/vr_reproject.cpp` | Atomiswave (Sammy 2004): its aim trim (see Findings). |
| Confidential Mission profile | `core/rend/vr_reproject.cpp` | US (MK-51160): its camera, life size and aim trim (see Findings). |
| Light guns for every game | `core/rend/vr_reproject.cpp` | In the headset ports A and B get light guns for every Dreamcast game, not only those with a profile. |
| Fixed clock | `core/hw/aica/aica_if.cpp` | `ACVR_RTC=<seconds since 1950>` fixes the Dreamcast clock so test runs repeat exactly (Confidential Mission seeds its randomness from it). |
| No desktop dialogs | `core/windows/winmain.cpp`, `core/sdl/sdl.cpp` | Flycast never shows a message box: its two error boxes are logged instead, Windows crash, critical-error and abort dialogs are off (`SetErrorMode`, `_set_abort_behavior`), SDL assertions are ignored, and a fatal error ends the program quietly when no debugger is attached. Checked: a missing disc ends the run with the reason in `flycast.log` and no window shown. |
| Wide preview | `core/rend/vr_reproject.cpp` | `ACVR_PREVIEW_WIDE=k` shows k times the stock view in the flat picture, the 2D plane still at its stock size: what the headset shows around the original frame (open-frame pictures). The shot log also gives the centre of each polygon near a shot. |
| No widescreen in the headset | `core/rend/transform_matrix.cpp`, `core/rend/vr_reproject.cpp/.h` | `rend.WideScreen` is ignored while the 3D rebuild runs in the headset (`vr::ignoreWidescreen`): it squeezed the picture to 3/4 and shifted it sideways there, and the light gun aimed in the unsqueezed picture (see Findings 2026-10-21). |
| Universal controls | `core/rend/vr/xr_host.cpp`, `core/rend/vr/acvr_chord.h`, `core/input/udp_lightgun.cpp` | The stick clicks are Start, Coin, recentre and quit (see Controls below); the menu buttons and left X no longer do anything. `lightgunSet` bit 9 presses the pad's D, the coin of Naomi and Atomiswave games. |
| Eye-buffer test | `core/rend/gles/gles.cpp`, `core/rend/vr_reproject.cpp` | `ACVR_EYE_SIZE=2592x2688` makes the renderer build its matrices as if it drew into a headset eye buffer of that size (a PC test of the line above); `ACVR_NO_WIDE_FIX=1` switches the fix off to see the old picture. |

The hidden window and every `ACVR_*` test aid change nothing unless their environment variables are set.

## Build on Windows

1. Clone the upstream at the pinned commit with its submodules, then apply the patch:
   `git apply patch/acvr-hotd2-vr.patch`.
2. Remove the lettering from the arcade gun: `python tools/clean_gun_model.py <source folder>` (numpy; it uses
   `assets/guns/src/arc_light_gun_mesh.py` from this repository and the fork's own `hotd2-vr/gun_model/convert_gun.py`).
3. Build with `tools/build.cmd <source folder> <vcvars64.bat> <cmake.exe> <ninja.exe>`. The
   upstream `hotd2-vr/build-win.cmd` uses the CMake and Ninja that come with Visual Studio; if
   only an MSYS2 CMake is available, `tools/build.cmd` adds the three flags that keep it from
   picking up MSYS2's own libraries and its missing `cmcldeps` (see the comments there).
   The configure step downloads the Khronos OpenXR SDK source pinned by checksum in the
   upstream CMakeLists.

## Play

Copy `build-win\flycast.exe` to a folder of its own (its settings and saves live beside it),
add an `emu.cfg` with `vr.Xr = yes` under `[config]`, start SteamVR, then:

    flycast.exe -config "config:vr.Xr=yes" "<your disc image>"

## Controls

Gun: either trigger fires, a shot away from the view reloads (the game's own off-screen reload), the other hand's
grip racks the HOTD2 pistol's slide, B or left Y is the gun's B (skip a scene, back out of menus, pump in the
Atomiswave games). The thumbstick is the gun's D-pad in menus; with a grip held, up and down sizes the world.

Universal layer (the same in every AladdinsCastle game; `core/rend/vr/acvr_chord.h`, identical to the copy in the
Model 2 viewer and the Supermodel patch):

| Gesture | Does |
|---|---|
| Left stick click, alone | Start (player 1's; with `vr.DualWield` and a game that shows the player 2 prompt, it lets player 2 join) |
| Right stick click, alone | Coin (the pad's D; Naomi and Atomiswave coin, nothing in plain Dreamcast games) |
| Both clicks, the second within 150 ms of the first, released within 1 s | Recentre |
| Both clicks held for 3 s | Quit through Flycast's normal exit (`dc_exit`, as closing the window), which saves the VMU and NVRAM; on Android builds it only logs, because there is no desktop exit |

A click waits 150 ms for the other, so Start and Coin arrive 150 ms late and never as part of a chord. Right A stays
an extra Start (it has no other use). No menu or system button is bound on any profile, and left X, left A (Index)
and the left menu button no longer do anything. Exceptions: the laser (M4) has no toggle, because right B and left Y
are the gun's B, which games need; set `vr.Laser`. On Vive wands (no stick) the pad clicks are the universal clicks
and the gun's B has no button. WMR keeps the pad click as the gun's B.

`tools/test_chord_copies.py` fails if the three copies of the resolver differ; the resolver test is
`setups/supermodel-vr/tools/chord_resolver_test.cpp`.

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
  The game's own shot flash agrees (its centre lands 7 to 32 px from where the gun was sent). Fixed: the US profile
  now sets those words (x left, centre, right 133, 398, 663 with offset 208; y top, centre, bottom 44, 140, 236). With them
  the game's own position is within 2 px of where the gun was sent over a 5 x 5 grid, and its shot flash lands on it.
- Open frame: with the profile's wider view (41 to 74 degrees) the game draws all around the original frame: 90-96%
  of the area outside it holds picture in chapter 1 (`ACVR_PREVIEW_WIDE=2`, `tools/openframe.py`).

- Ranger Mission (Atomiswave), offline: boots to play on free play (set once in the system menu's COIN SETTINGS
  and kept in the game's saved settings), A starts, a shot picks the stage and skips the intro. It reads the gun
  every frame at 0x0C7008B4 / 0x0C7008B8 (centred pixels) with a fixed mapping of its own: up to 26 px off at the
  right edge and 20 px low. With its profile's aim trim its reading is within 4 px over a 5 x 5 grid, and its bullet
  puffs land on the aim points. Shots against two identical control runs change its ammo bar; a shot off the
  screen refills it. 3D: a view turned 30 degrees shows the jungle beyond the original frame. Open frame: with
  Flycast's `rend.WideScreen` it draws the jungle past 4:3 on both sides (95% of the side area has picture), and
  through the 3D rebuild above and below too.

- The other Atomiswave gun games, offline (2026-10-19). Each needs a one-time set-up in its own system menu, kept in
  its saved settings (`<set>.zip.nvmem` in Flycast's data folder): free play (COIN SETTINGS, or GAME SETTINGS >
  CREDIT SETTINGS on the Sega titles) and the gun's own calibration in TEST MODE > I/O TEST (shoot the target or
  the centre mark with each gun). Flycast's light gun needs it: Sega Clay Challenge read the gun 105 px to the right
  before. After it, each game's own gun read-out or hit marker lands within 1-3 px of where the gun was sent:
  Sports Shooting USA (bullet holes on a wall, 9 points), Extreme Hunting (its I/O test read-out), Extreme Hunting 2
  (its GUN ADJUST cursor at three points), Sega Clay Challenge (cursor at four points, hit marker at 7 of 9 points).
  Shots against two identical control runs change each game. Extreme Hunting 1 and 2 pump after every shot and
  Clay Challenge reloads after every shot: B or a shot off the screen does it (the I/O test shows PUMP on for
  both). Sports Shooting USA reloads by itself. Every one draws past 4:3 with Flycast's `rend.WideScreen`
  (89-100% of the side area has picture) and shows its world beyond the frame in a turned 3D view.
  `tools/aimpump.py` makes a nine-point script that pumps before every shot; `tools/holes.py` finds bullet holes.

- Confidential Mission (US), offline (2026-10-20), after the owner's headset report (aim drifting, no off-screen
  reload, open frame only partly). Causes found: no light gun in port A without a profile (now always); no camera
  profile, so the world was rebuilt at HOTD2's scale, 20-60 cm from the eyes and behind the gun's muzzle, so the
  aim ray started past what it was meant to hit and a gun pointed away from the screen still "hit" the miniature
  room (no off-screen shot). Its profile now has its camera (projection 0x0C2F0C20, viewport 0x0C2F0D58, focal
  540.8), its size (`unitScale` 4.6: a guard 340 px tall at W 25 is 1.8 m) and an aim trim: it reads the gun at
  0x0C2AFCB8/BC as 322.5 + 1.073 (x - 320), 229 + 1.152 (y - 240), up to 34 px off; with the trim within 2 px
  over a 5 x 4 grid. Shots vs two identical control runs (with `ACVR_RTC`) hit the guard; a shot off the screen
  reloads at once. Open frame: with `rend.WideScreen` it draws the rooms past 4:3 (92-100% of the side area in
  play, 96-100% around the frame through the 3D rebuild); in story scenes people at the sides can be cut into
  pieces (the game culls body parts). A wider game view (`vr.FovScale` 1.5 through its viewport) also fills
  60-100%, but its gun hit test against the widened view is untested, so it is not used.

- Sports Shooting USA, after the owner's headset report (2026-10-21: "the aim was off"; open frame "wasn't working
  right: too many graphics outside the frame"). Two findings.
  1. The game's own reading is fine: with the gun calibrated in its I/O TEST its read-out is within 4 px at nine
     points over the screen (the calibration targets are at 31.5, 39.5 and 607.5, 359.5, and the game only counts
     a gun inside that rectangle as on screen: a shot below about y 360, the HUD strip, reloads, and one left of about x 30,
     right of about 610 or above about y 40 is off screen too), and nine bullet holes on the practice wall land within 3.1 px of the aim
     (rms 2.3, mean -0.9 / +1.4), in the 4:3 picture and in the wide one alike. Within the 0.01 of the screen the
     lane allows, so it has no aim trim.
  2. The open-frame view was the problem. Its launcher entry set `rend.WideScreen`; the headset path builds the
     game's picture through the renderer's own matrices, and with WideScreen those stretch the frame to 16:9
     and re-centre it for the size of the buffer rendered into. On the PC the picture is 16:9, so it cancelled out
     (every earlier open-frame picture looked right). In the headset's near-square eye buffer (2592 x 2688 in the
     owner's log) the picture came out squeezed to 3/4 and moved left: x_seen = 0.75 x - 122.5 of 640 (measured by
     registering the picture against the plain one, `tools/register_x.py`: the same constants in every scene of
     five games). The hit test and the 2D plane use the unshifted picture, so aiming at what you saw
     was off by -138, -203 and -268 px at x 60, 320 and 580 (many of the owner's shots in the headset log are at x 100 to 200: the
     centre of the picture was seen at x 117). `vr::ignoreWidescreen()` keeps the matrices 4:3 in
     the headset; with `ACVR_EYE_SIZE` and `rend.WideScreen=yes` the same registration gives x_seen = x, error 0, in
     all five Atomiswave games tested (Sports Shooting USA, Ranger Mission, Extreme Hunting 1 and 2, Sega Clay
     Challenge). Confidential Mission's open-frame launcher entry set the same flag, so it had the same error.
     The 3D view (no WideScreen) was never affected: its registration is exact with and without the fix.
  The rebuilt 3D view already shows what the game draws beyond the frame, so after the fix "open frame" and "3D"
  are the same picture. What is out there (rip passes, `tools/outside_frame.py`, the area up to one half frame
  beyond each side): in play 97 to 100% real scenery (ground, sky, hills, W 700 to 4000) and the HUD stays inside
  the frame; on the title, the category and division menus and stage changes 0 to 41% (81% with the black fade
  slab) is the menus' own art as perspective polygons with streaks and black slabs, which can not be put on
  the 2D plane (a flat-screen profile changed nothing: the polygons are not flat in W). The launcher entry for
  this game keeps only the 3D view.
- Older lines above that say the open frame came "with `rend.WideScreen`" describe the flat PC picture; in the
  headset that flag was the bug above.

## Historical status (2026-10-14)

The snapshot below predates the 19–21 October findings above. In particular,
Confidential Mission now has the profile listed in the patch table and an offline
check described under Findings. The old "no profile" and "not yet checked" lines
are not current status. See [current development status](../../docs/status/current.md)
for the distinction between offline checks and headset acceptance.

- The House of the Dead 2 (US): plays in the headset with the upstream profile.
- Death Crimson OX (US): plays in 3D; black bars fixed and checked off-headset in a cutscene.
- Confidential Mission (US): boots; no profile; its memory-card menu defeats scripted presses.
- Gun animations: built, not yet seen in a headset.
- 2026-10-19: the Hub (and the private launchers) plug light guns into ports A and B for every Dreamcast game:
  the build only does that itself for games it has a profile for, so Confidential Mission had no gun in the headset.
  Not yet checked in a run (the test PC's graphics card failed during the session).
