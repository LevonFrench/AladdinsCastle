# PlayStation light-gun games in the headset (acvr-ps1)

`acvr-ps1` is a small program of ours that runs a PlayStation emulator core
in the libretro format and shows its picture in the headset as a big theatre
screen. The right VR controller is the light gun: the game receives it as a
Namco GunCon or a Konami Justifier, exactly as if one were plugged in.

It brings no emulator, BIOS or game. You point it at your own:

- **Core:** Beetle PSX (`mednafen_psx_libretro.dll`, GPL-2.0), for example
  the one a RetroArch installation already has in its `cores` folder.
- **BIOS:** a folder holding your own `scph5501.bin` (or `scph5500.bin`,
  `scph5502.bin`), the names Beetle PSX looks for.
- **Disc:** your own CHD, PBP or CUE/BIN.

## Controls

| VR controller | Game |
|---|---|
| Right controller pointed at the screen | Gun aim |
| Right trigger | Trigger. Pulled while pointing off the screen: reload |
| Right A | GunCon A (Justifier: its start button; arcade: Start) |
| Right B | GunCon B (arcade: Coin) |
| Right grip | GunCon A as well: the pedal. In Time Crisis hold it to step out and shoot, let go to take cover and reload |
| Hold left Y for 3 s (Index: left B) | Quit, saving the memory card |
| Left stick | Pad d-pad (a standard pad on the other port, for menus and pad-only modes) |
| Left trigger, left X (Index: left A), left grip | Pad Cross, Circle, Square |
| Left stick click (also the left menu button, which SteamVR keeps for its dashboard under ALVR) | Pad Start |
| Home key, or both grips held for about 1 s, when the game uses no grip (see below) | Put the screen in front of you again |

Each trigger pull gives a short vibration and a small visual kick of the gun.
Escape on the PC also quits cleanly. A launcher or the Hub can quit it by
creating the file named in `ACVR_PS1_STOP_FILE`.

SteamVR's **Exit game** closes it too, saving the memory card. The game pauses,
picture and sound, whenever the headset is not showing it: the SteamVR
dashboard is open, the headset is idle or in standby, or the session is still
starting. It carries on where it stopped when the picture comes back. If the
headset picture stops for good (no runtime, or the runtime fails), the game
saves and quits with exit code 9 instead of running unseen; the reason is in
`acvr-theatre.log`. If the sound device is lost, the game carries on silent,
paced by the clock. A game in the headset runs at normal priority; offline
runs (`--headless`, `--no-xr`) run below normal.

## Theatre

Everything you see is drawn into one OpenXR projection layer per eye: the
game picture on a screen curved around you, the gun model in your hand and
an orange aim dot where the gun's ray meets the screen, all depth-tested, so
the gun is always in front of the picture. (SteamVR draws quad layers over
projection layers whatever their order, which hid the gun behind the picture
in the PS2 screen mode.)

The picture is sampled "sharp bilinear": every game pixel stays a crisp
square and only its edges are blended, over one headset pixel. When the
picture is smaller than the headset pixels it uses mipmaps instead.

| Setting | Default | |
|---|---|---|
| `ACVR_XR_DISTANCE` | 2.5 | metres to the screen |
| `ACVR_PS1_SCREEN_W` | 2.4 | screen width in metres |
| `ACVR_PS1_CURVE` | 1 | 1 curves around you, 0 is flat, between is gentler |
| `ACVR_PS1_LIFT` | 0 | screen centre height above your eyes, metres |
| `ACVR_PS1_CRT` | 0 | 1 adds scanlines |
| `ACVR_PS1_SBS` | 0 | 1 when the core itself draws both eyes side by side in one picture (left eye in the left half) |
| `ACVR_PS1_SWAP_EYES` | 0 | 1 swaps the two halves between the eyes, if far things come towards you instead of receding |
| `ACVR_PS1_LETTERBOX` | worked out | share of the picture's rows cut from the top and from the bottom of each side-by-side eye. Unset, the host works it out from the core's frame size and reported aspect and re-reads it whenever the core changes its geometry; a value here overrides it |
| `ACVR_PS1_AIM` | 1,0,1,0 | per-game aim trim: x scale, x shift, y scale, y shift around the centre, measured against where the game registers the shot |
| `ACVR_PS1_HIDE_FLASH` | 1 | keep the games' per-shot white flash frames out of the headset (the game still draws them and reads the gun). Only white frames while the trigger is held or within 20 frames after it count, so menus and fades always show |
| `ACVR_PS1_CHEAT_FILE`, `ACVR_PS1_CHEATS` | none | a DuckStation-format .cht file and the `;`-separated entries to enable |
| `ACVR_PS1_GUN_MODEL`, `ACVR_PS1_GUN_META` | none | the gun's `.glb` (`assets/guns`) and `.toml` (`data/guns`) |

The screen is placed level, straight ahead of where you look when the first
picture arrives; Home places it again, and so does holding both grips for about
a second (in the headset, where there is no Home key; let go and hold again to
repeat). The both-grips gesture is on only when the game does not use a grip:
always in gun mode, and in the pad modes only when the active `ACVR_PAD_MAP`
(or the aim-hold control) names neither `rgrip` nor `lgrip`. The face-button
layout (`ACVR_PS1_FACE_BUTTONS=1`) puts the gun's buttons on the grips, so it
turns the gesture off too. The default pad, wheel and aim maps all use a grip,
so there the screen is placed at the start and by the Home key on the PC; a
map such as Star Fox 64 3D's, which holds boost and brake together, never moves
the screen. (`theatre::Config::gripRecentre`, set from `pad::Config::usesGrip`.)

## Saves

Memory card 1 is the core's save memory. The program loads it from
`<saves>\<disc name>.srm` at start, writes it every 10 seconds when it has
changed and again on quit, always through a temporary file and a rename.

## Build

```
powershell -File build-host.ps1 -OpenXRInclude <folder with openxr\openxr.h> -OutputDirectory <private folder>
```

It needs MSYS2 MINGW64 `g++`. It borrows the hidden-window and bounded-wait
helpers of the Model 2 viewer (`..\mame-model2-vr\presenter`). Keep
`openxr_loader.dll` beside the program. Without a VR runtime the program still runs: the
presenter stays off.

## Run

```
acvr-ps1 --core <dll> --game <disc> --system <BIOS folder> --saves <folder>
         [--gun guncon|justifier] [--port 1|2] [--option key=value]...
```

`profiles.json` is superseded by the per-game files in `profiles\` (see
Profiles below) and stays only until its remaining entries are migrated; nothing
reads it.
`--option` sets a core option, such as
`--option beetle_psx_internal_resolution=2x`. The program writes every
option the core offers, with the value in use, to `core-options.txt` in the
saves folder.

## Profiles

A profile is one TOML file per game in `profiles\`, named by the catalog id
(`3ds-star-fox-64-3d.toml`, `ps1-time-crisis.toml`). It holds what makes the
game play right: the host's `ACVR_*` settings, the core options, the pad map,
and named views. It holds no core, game, BIOS, save folder or other path of one
machine; those stay on the command line, from the launcher.

```
acvr-ps1 --profile profiles\3ds-star-fox-64-3d.toml [--view swap-eyes] --core <dll> --game <dump> --system <folder> --saves <folder>
acvr-ps1 --profile profiles\3ds-star-fox-64-3d.toml --print-config
```

Precedence, lowest to highest: the program's own default, the profile, the
profile's chosen view (`--view`, else its `default_view`), the environment
(`ACVR_*`), the command line (`--option`, `--controls`, `--gun`, `--port`). So
running with no profile behaves as before, and an exported `ACVR_*` variable
still wins over a profile for a quick trial. The host logs the resolved
settings once at start, each with where it came from, and `--print-config`
prints all of them and exits without starting a core, a window or the headset
runtime.

The file's keys (anything else is an error, so a typo fails loudly):

| Key | |
|---|---|
| `schema = 1`, `name`, `notes` | required version, a title for the log, free text |
| `system` | `ps1`, `saturn`, `n64` or `3ds`; a setting that does not apply to it is warned about |
| `controls`, `gun`, `port` | as `--controls`, `--gun`, `--port` |
| `default_view` | the view used when `--view` is not given |
| `[settings]` | `ACVR_*` names exactly as in `host\HostConfig.h`; `"auto"` leaves a setting to the host (the letterbox of a 3DS frame) |
| `[core_options]` | `--option` lines: `citra_factor_3d = 40` |
| `[pad]` | `map = { rtrig = "a", ... }` or a string, plus `wheel_deg`, `aim_hold`, `aim_button`, `aim`, `aim_curve`, `aim_dot`, `aim_on`, `follow_u`, `follow_v`, `follow_gain` |
| `[views.<name>]` | `label`, `evidence`, `notes`, and its own `settings`, `core_options` and `pad` tables, layered over the base |

Every setting the host reads is listed once in `host\HostConfig.h` with its
type, default and the systems it applies to. It warns about any `ACVR_*`
variable in the environment that nothing reads (with the closest name), and
about a value of the wrong type. A profile may not set a path (the stop file,
gun model, cheat file, trace folder). Settings read by our patched cores
(`ACVR_PS1_STEREO*`, `ACVR_SAT_STEREO*`) are listed too; the host passes them on
because the profile is applied before the core loads.

The host has no TOML dependency (the Hub's toml++ comes from CMake's
FetchContent; this program is one g++ command line), so `HostConfig.h` carries a
small strict reader for the subset the profiles use: `[a.b]` tables,
`key = value`, one-line `"strings"`, `'literal strings'`, integers, floats,
`true`/`false`, `[arrays]`, `{ inline = "tables" }` and `#` comments. Dotted
keys, multi-line strings or arrays, `[[arrays of tables]]` and dates are refused
with the line number. Checks: `python test_host_profiles.py` (registry against
the code, profiles against the registry and the catalog) and
`host\HostConfigTest.cpp` (the reader and the precedence; needs g++).

## Offline proofs

`--headless --frames N --script <file> --snapshots <folder>` runs without any
window, sound or VR runtime, as fast as the PC allows, and saves pictures.
The script has one event per line, timed in emulated frames:

```
600 aim 0.5 0.5     # gun at the screen centre (0..1 from the top left)
1500 pad-start 6    # the pad on the other port: pad-up/down/left/right/cross/circle/square/triangle/start/select
720 trigger 4       # hold the trigger 4 frames
900 a 6             # also: b, start, select, offscreen N, reload
1200 snap title     # save the picture as title.png
```

In these runs the core draws its own crosshair, which shows where the game
believes the gun points. Add `--theatre-proof <prefix> --test-aim U V` and,
after the run, both eyes of the theatre are rendered with no VR runtime, from
a fixed head pose and a right hand aimed at (U, V), into `<prefix>-left.png`
and `<prefix>-right.png`. The log says where the aim ray met the screen,
which must equal (U, V).

Results so far (Beetle PSX 0.9.44.1, hidden runs): Point Blank was played
from the intro to Training stages using only the gun (calibration, menus,
hits counted by the game, memory card written). Thirteen other gun games
boot; Time Crisis, Project Titan, Point Blank 2 and Gunfighter pass their gun
calibration screens. Headset viewing, the real controller and vibration have
not been tried yet.

## Arcade sets through the MAME core

The same program runs arcade light-gun sets through the MAME libretro core
(`mame_libretro.dll`) with `--gun lightgun --option mame_lightgun_mode=lightgun`.
Right B inserts a coin and right A is Start. MAME keeps its NVRAM and
settings under the saves folder. Point Blank 2 (`ptblank2`, System 12) was
played this way in a hidden run with MAME 0.285: coin, course chosen by
shooting, targets knocked down. Point Blank (`ptblank`) needs files the
current MAME set expects (the C75 sound BIOS and a newer sprite ROM).

## Sega Saturn gun games (Beetle Saturn)

The same program runs Saturn light-gun games through the Beetle Saturn core
(`mednafen_saturn_libretro.dll`, GPL-2.0) with `--gun stunner`: the gun port
gets the core's Stunner device (the US name of Sega's Virtua Gun; the core
treats both the same). The BIOS folder holds your own `mpr-17933.bin`
(US/Europe) and `sega_101.bin` (Japan). In offline runs the core draws its
crosshair (`beetle_saturn_virtuagun_crosshair`); in the headset it is off.
Pulling the trigger while pointing off the screen is the Stunner's
off-screen shot, which these games use to reload. Right A is the gun's
Start button.

Each game has its own gun calibration screen (Options, Gun Adjust or
Calibrate Gun), and before calibration both Virtua Cops register a shot
above the aimed point (Virtua Cop: 13% of the picture height; Virtua Cop 2:
10%, and 9% of the width to the right), so the bottom edge (and for
Virtua Cop 2 the left edge) cannot be reached. Calibrating once by
shooting the drawn centre of the target puts every shot within 0.004 of the picture of the aimed point, measured with the
game's own bullet holes on its shoot-test screen; no aim trim is needed.
Virtua Cop saves its calibration in backup memory, so its launcher brings a
first save with the calibration done (never replacing an existing save).

The House of the Dead moves its sight towards the gun in steps, and the sight stops up to about 8 pixels
short when the gun comes from the right or from above, wherever the
calibration is. The launcher therefore sets an aim trim of half that step
(`ACVR_PS1_AIM=1,-0.0114,1,0.0156`), which leaves at most 0.012 of the width
and 0.02 of the height in either direction instead of 0.023 and 0.034 in
one. Its shot flash whitens the scene rather than painting it white, so
`ACVR_PS1_FLASH_SHARE=60` (the share of bright samples that makes a flash,
default 85) keeps all of them out of the headset. The disc named USA in our
set is the European release (50 Hz, refused by a US console), so the
launcher uses the Japanese disc, whose menus are in English. Like Virtua
Cop it keeps its calibration in backup memory.

Area 51 (Configure 1P, Calibrate) has the same kind of sight steps (up to
0.025 of the picture short from one side); its launcher trim is
`ACVR_PS1_AIM=1,-0.0125,1,0.0055`, leaving at most 0.021. Its shot flash
is a blue frame, so `ACVR_PS1_FLASH_CHANNELS=b` (the colour channels that
must be bright; default `rgb`, white) keeps those out of the headset.

`--state <file>` starts from a saved moment. Virtua Cop 2, Area 51 and
Hakaider keep a gun calibration that they did not restore from their saves after a restart in
our runs, so their launchers start from the title or main menu just after
the game's own calibration.

Mechanical Violator Hakaider (Japan) calibrates in Option, Gun Adjust
(the left trigger opens it): the first shot at the drawn centre
calibrates, later shots leave holes, which landed within 0.005 of the aimed
points. Its launcher starts at the main menu just after that calibration.
Its menus take the gun trigger as confirm and the pad to move.

The Saturn draws its 3D as flat distorted sprites: the game's own code
projects every polygon, and VDP1 receives four 2D screen corners per quad
with no depth. Beetle Saturn, Kronos and YabaSanshiro all draw from those
corners; the only depth in the Kronos and YabaSanshiro OpenGL renderers is
the VDP2 layer priority (0 to 7), and YabaSanshiro's perspective-corrected
polygon mode derives its correction from the quad's shape. With no depth
buffer, a game paints far to near, so draw order is the only depth cue left
(as for the PlayStation diorama); see the experiment below. Only Beetle Saturn has the Stunner, and none of the
three cores has a widescreen mode, so there is no open frame either: these
games play on the theatre screen.

### Depth from draw order (experiment, off by default)

`beetle-saturn-depth.patch` (against the Beetle Saturn commit in
`beetle-saturn-pin.json`, GPL-2.0 like the core) tries to recover depth the
way the PlayStation diorama does. With `ACVR_SAT_STEREO=1`:

- Every VDP1 drawing command gets a number in the order it is drawn, and
  every pixel it draws keeps that number in a side buffer beside the VDP1
  framebuffer. Normal and scaled sprites (HUD, text) stay on the screen
  plane; a command covering 40% of the screen is a backdrop, far away; the
  others (distorted sprites, polygons, lines) go from far (drawn first) to
  `ACVR_SAT_STEREO_POPOUT` (0.3) in front of the screen (drawn last), by their
  rank among the frame's 3D commands (`ACVR_SAT_STEREO_DEPTH=size` uses their
  screen size instead).
- VDP2 records which layer won each output pixel. A VDP2 layer over a sprite
  pixel (HUD) sits on the screen plane; one with no sprite under it is far at
  the top of the picture and comes to `ACVR_SAT_STEREO_FLOOR` (0.15) at the
  bottom, for floors drawn as a rotating background.
- The core outputs both eyes side by side (`ACVR_PS1_SBS=1` in `acvr-ps1`):
  the left eye exactly as the game drew it, so the light gun maps to it
  unchanged, and the right eye with each pixel moved by its depth (the eyes'
  separation on the theatre screen at infinity); holes take the farther
  neighbour. `ACVR_SAT_STEREO_DEBUG=1` shows the depth map as the right eye.

Results (hidden offline runs): the gun is unchanged (Virtua Cop's
calibrated shots in the left eye within 0.002 of the aimed points, as
without the patch). In Virtua Cop the layering reads correctly most of the
time (sky and hills far, the floor ramping towards you, buildings in
between, enemies near, the HUD on the screen), but one object can get
several depths, the last polygons drawn always come out in front even when
they are a parked car in the middle distance, and edges show stretched
pixels. In The House of the Dead the draw order does not follow depth
across the frame (floors and walls are drawn late and come out in front of
the zombies standing on them), so large areas get the wrong depth. Not good enough to use; the launchers keep the theatre screen.

Results so far (Beetle Saturn 1.29.0 b4df47a, hidden offline runs):
Virtua Cop, Virtua Cop 2, The House of the Dead, Area 51 and Hakaider
play from the title into their first stage with the gun only; the same
run with and without trigger pulls gives different pictures (enemies hit and
points scored only with shots)
while two runs without shots are identical, the off-screen shot reloads, and
their flash per shot (one white frame; two in The House of the Dead; one
blue frame in Area 51) is kept out of the headset.

## Real 3D: the SwanStation stereo patch

The PlayStation does its 3D maths on the CPU, so its graphics chip only ever
receives flat screen positions. SwanStation's PGXP keeps each vertex's depth
(`w`). `swanstation-stereo.patch` (against the commit in
`swanstation-pin.json`, GPL-3.0 like SwanStation) uses it to draw every
picture twice in the OpenGL renderer:

- The right eye has its own copy of PlayStation video memory, its depth, its
  read copy and its display texture. Every VRAM write, fill, copy and
  depth/read-copy update runs for the right eye first as a replay that leaves
  the emulator's bookkeeping untouched, then for the left eye as normal, so
  both copies see identical state. Readbacks to the console use the left eye.
- Each draw runs once per eye. The vertex shader moves a vertex sideways by
  `shift x clamp(1 - conv / w, -popout, 1)`, where `shift` is half the eyes'
  separation on the theatre screen. Far scenery therefore never needs the
  eyes to diverge; vertices without PGXP depth (HUD, text, 2D) stay on the
  screen plane.
- The core outputs both eyes side by side; `acvr-ps1` shows the left half to
  the left eye and the right half to the right eye (`ACVR_PS1_SBS=1`).
- The light gun keeps mapping to one eye's picture.

| Setting | Default | |
|---|---|---|
| `ACVR_PS1_STEREO` | 0 | 1 turns it on (OpenGL renderer only) |
| `ACVR_PS1_STEREO_CONV` | auto | the PGXP depth `w` that sits on the screen; `auto` eases it each frame toward a low percentile of the scene's depth |
| `ACVR_PS1_STEREO_AUTO` | 0.3 | that percentile: this share of the geometry may come in front of the screen |
| `ACVR_PS1_STEREO_DEPTH` | 1 | multiplier on the effect |
| `ACVR_PS1_STEREO_POPOUT` | 2 | how far in front of the screen things may come, in units of the far limit |
| `ACVR_PS1_IPD`, `ACVR_PS1_SCREEN_W` | 0.064, 2.4 | metres; the same screen width as the theatre |
| `ACVR_PS1_STEREO_STATS` | 0 | 1 logs a histogram of `w` every few seconds, to choose `CONV` per game |

Measured on Time Crisis gameplay (2x resolution, `CONV` 0.05, hidden
offline runs): most geometry has `w` between 0.03 and 0.09; disparities run
from about 14 px crossed (nearest objects about 1.2 m away on a 2.5 m screen)
to 13 px uncrossed, which is exactly the eye separation, so nothing is beyond
infinity; the HUD sits on the screen. Not yet seen in a headset.

With the automatic zero plane (the default) the same Time Crisis scenes
measured almost the same as a hand-picked `CONV` of 0.05: scene median 5 to
9 m behind the screen, nearest objects 1 to 2 m away, far limit at the eye
separation. No per-game tuning is needed.

### Diorama for sprite games (prototype)

`ACVR_PS1_DIORAMA=1` (with stereo on) gives primitives that have no PGXP depth
a depth from the frame's draw order, because the PlayStation paints back to
front. Rules, in order: primitives drawn into a small off-screen work area are
left flat (they are often copied in later); backdrops (anything covering 40%
of the picture, spanning 60% of its width or height, drawn in the first fifth
of the frame, or tile-sized in its first 60%) share one layer behind the
screen; the last tenth of the frame (usually the HUD) stays on the screen;
everything between moves from behind the screen to in front of it in draw
order. Shifts are whole PlayStation pixels, so sprites never sample a
neighbouring tile's edge. `ACVR_PS1_DIORAMA_FAR` / `_NEAR` (0.6 / -0.6) set
the range.

On Point Blank's piranha stage the water sits about 12 px behind the screen,
the fish come 4 to 8 px in front, and the score and timer stay on the screen.
Two faint one-pixel seams remain in some frames. Not yet seen in a headset.

## Nintendo 64 games (mupen64plus-next)

The same program runs Nintendo 64 games through the prebuilt
`mupen64plus_next_libretro.dll` (GPL-2.0, GLideN64 renderer) with
`--controls pad|wheel|aim` instead of a light gun: port 1 becomes a pad with
an analog stick, fed from the VR controllers (`host/PadControls.h`). The
core's RetroPad layout: B is the N64 A button, Y is B, L2 is Z, L and R are L
and R, the right analog stick is the C buttons.

| Mode | Right controller | Used for |
|---|---|---|
| `wheel` | Racing: the left stick steers, the triggers are gas and brake (per the map). Rolled like a steering wheel it also steers only with `ACVR_N64_ROLL_STEER=1` (off by default, the owner's choice): analog X, linear from a 3 degree dead zone to full lock at `ACVR_PAD_WHEEL_DEG` (75) | Mario Kart 64, F-Zero X and the other racers |
| `aim` | While the right grip is held (`ACVR_PAD_AIM_HOLD`, or `always`), where it points on the screen sets the analog stick, with the game's aim button held (`ACVR_PAD_AIM_BUTTON`, N64 R), so the game's own crosshair follows it | GoldenEye 007 |
| `pad` | Buttons only; the left stick is the analog stick | anything else |

`ACVR_PAD_MAP` maps VR controls to RetroPad buttons, for example
`rtrig=b,ltrig=rs-down,rgrip=r,lgrip=l2` (F-Zero X: right trigger accelerates,
left trigger brakes on C-down, the grips lean). VR names: `rtrig ltrig rgrip
lgrip ra rb lx ly lmenu rclick lclick rup rdown rleft rright`
(`lclick`, the left stick click, is Start in every default map; `lmenu` and,
where the right stick is unused, `rclick` are aliases); RetroPad names:
`b y select start up down left right a x l r l2 r2 l3 r3` and `rs-up rs-down
rs-left rs-right` (the right stick pushed fully). `ACVR_PAD_AIM` is the aim
trim `x scale, x shift, y scale, y shift` from the screen point to the stick,
measured per game against its own crosshair. The game's Rumble Pak
(`--option mupen64plus-pak1=rumble`) pulses the right controller. Saves
(EEPROM, SRAM, controller pak) are the core's save memory, kept in the saves
folder like the PlayStation memory card.

Default maps (`PadControls.h`, `DefaultMap`; `ACVR_PAD_MAP` replaces the whole map):

| Mode | Map |
|---|---|
| `pad` | `ra=b,rb=y,rtrig=l2,rgrip=r,ltrig=l2,lgrip=l,lx=b,lclick=start,lmenu=start` (no `rclick`: the right stick is the C buttons) |
| `wheel` | `rtrig=b,ltrig=y,rgrip=r,lgrip=l2,ra=b,rb=y,lx=l,lclick=start,lmenu=start,rclick=start` |
| `aim` | `rtrig=l2,ra=b,rb=y,lgrip=l2,lx=l,lclick=start,lmenu=start,rclick=start,ltrig=r` |

The left stick click is Start, so L is no longer there: in `wheel` and `aim`
L is on left X (Index: left A), where it only duplicated Z (`wheel`) or A
(`aim`). In `pad` L stays on the left grip.

Offline scripts drive a virtual VR controller through the same mapping:
`roll DEG`, `point U V`, `rtrig V`, `ltrig V`, `rgrip V`, `lgrip V`,
`lstick X Y`, `rstick X Y`, `vr <control> N`, plus raw `btn <RetroPad name> N`,
`stick X Y` / `stick off` and `cstick X Y` for menus. `state-save <file>` and
`state-load <file>` start proofs from a saved moment.

Open frame: the core's `mupen64plus-aspect=16:9 adjusted` widens the 3D
projection; games whose culling allows it draw real geometry outside the 4:3
centre (GoldenEye, Mario Kart 64). F-Zero X needs its widescreen code from
RetroArch's cheat database with `mupen64plus-aspect=16:9`:
`ACVR_PS1_CHEAT_FILE` also reads RetroArch `.cht` files, where an entry is
named by its description (`Widescreen`) or `cheatN`.

### 3D without changing the core

`host/GlStereo.cpp` gives a prebuilt OpenGL core real stereo from the outside
(`ACVR_GL_STEREO=1`). The host already hands the core its OpenGL entry points
(`get_proc_address`); the OpenGL 1.1 calls the core links directly are
redirected in the loaded core's import table. Then:

- Every framebuffer the core draws into gets a right-eye twin of the same size
  and format. Each draw, clear, blit and framebuffer-to-texture copy runs twice:
  as asked (left eye), then into the twins, with every bound texture that is a
  render target swapped for its twin. The final copy to the host therefore
  makes the right eye's picture too; readbacks to the console stay left.
- GLideN64's triangle vertex shaders get one line appended, the rule of the
  SwanStation patch: `x += eye * shift * clamp(1 - conv / w, -popout, 1) * w`.
  Orthographic geometry (w exactly 1: HUD, sprites) and 2D rectangles stay on
  the screen; 2D drawn into the 3D picture before its first depth-tested draw
  of the frame (skies, backgrounds) goes to infinity.
- `shift` is half of `ACVR_PS1_IPD / ACVR_PS1_SCREEN_W` of the picture width,
  whatever viewport the core uses. `ACVR_GL_STEREO_CONV` sets the `w` on the
  screen plane (default `auto`: a percentile, `ACVR_GL_STEREO_AUTO`, of a
  histogram of `w` the shaders count each frame); `ACVR_GL_STEREO_POPOUT` (2)
  and `ACVR_GL_STEREO_DEPTH` (1) as for SwanStation; `ACVR_GL_STEREO_STATS=1`
  logs the histogram. The shader cache must be off
  (`mupen64plus-EnableShadersStorage=False`) so the shaders are compiled from
  source. Both eyes go to the headset side by side; offline pictures are
  side by side too.

### HUD on its own panel, and a world wider than the screen

`ACVR_GL_HUD=1` splits the game's 2D overlay from its 3D world, the first of
the three things in `docs/3d-mode-target.md`. The same twin mechanism makes a
third picture, the HUD layer: in the framebuffer that holds the game's 3D
picture, every vertex is classed as world (perspective) or 2D; 2D drawn before
the frame's first depth-tested 3D draw is a backdrop (sky, part of the world),
later 2D is HUD. Fragment shaders get a guard that drops HUD fragments from the
world pictures and world fragments from the HUD layer, and gives HUD fragments
an alpha from the core's own blending (GLideN64 blends with dual-source
factors), so the layer is premultiplied colour over transparent black. The
theatre shows it on a flat panel fixed in the room with the screen, nearer and
a little lower: `ACVR_HUD_DISTANCE` (1.8 m), `ACVR_HUD_LIFT` (-0.08 m),
`ACVR_HUD_WIDTH` (default: the same view angle as the game's picture on the
world surface, so a crosshair lines up), `ACVR_HUD_SCALE`, `ACVR_HUD_OPACITY`.
In `aim` mode the controller then aims at the HUD panel, where the game draws
its crosshair. Offline snapshots also write `<name>-hud.png` (the layer over a
checkerboard).

`ACVR_GL_WIDE=kx,ky` squeezes the 3D projection about the picture's centre, so
geometry the game draws past the old edges comes into the picture (1/kx of the
width, 1/ky of the height); the theatre stretches it back on a surface that
much wider (`ACVR_PS1_SCREEN_W` is its width: 7 m at 2.5 m curves about 160
degrees round you), with no frame and edges fading into the dark
(`ACVR_SCREEN_FADE`, 0.06; `ACVR_SCREEN_FRAME=1` brings the frame back). Raise
the core's resolution with it (`mupen64plus-169screensize=1920x1080`), since
the picture is spread wider. How far a game draws is the game's own culling.

How far a game draws past its frame is the game's own business: GLideN64
clips to the game's projection, so squeezing shows more only where the game
already draws more (GoldenEye, Perfect Dark, F-Zero X: about twice the width).
Mario Kart 64 clips at its frame. For it, and for Wave Race 64 and Star Fox 64
(which have the same kind of code), a widescreen code that sets a wider
projection aspect (RetroArch's `Widescreen` entry with its aspect value 1.78
raised to 3.55) makes the game draw 32:9 itself, and `ACVR_GL_WIDE_SQUEEZE=0`
leaves the squeeze to the game.

Beetle Adventure Racing! breaks with `16:9 adjusted` even without any of this
(its frame-buffer copies tile the picture), so it runs 4:3 with the HUD panel
and 3D but no open frame. Its 2D layer has opaque black border bars;
`ACVR_HUD_KEY_BLACK=1` shows pure black HUD pixels as transparent.

Cruis'n Exotica draws its whole 3D world without the depth test;
`ACVR_GL_3D_NO_DEPTH=1` counts its triangle draws as the 3D picture all the
same, so its HUD still splits off.

### Aim that moves by rate (Star Fox 64)

Where the stick moves the game's marker rather than placing it,
`ACVR_PAD_FOLLOW_U` / `_V` give the marker's place on the picture from the
game's memory (`c0,addr:k,addr:k`, fitted offline against the marker in
snapshots, with the script line `ram-dump <file>` and `ACVR_RAM_WATCH`), and
the pad steers it to where the controller points with
`ACVR_PAD_FOLLOW_GAIN = px,dx,py,dy,ix,iy` (proportional, damping, integral).
`ACVR_PAD_AIM_ON=world` aims at the world surface instead of the HUD panel,
for a marker that is part of the 3D world.

`ACVR_GL_TRACE=<first frame>,<frames>` (with `ACVR_GL_TRACE_DIR`) logs what a
core draws and saves its vertex shaders, to study another core.

Results so far (Mupen64Plus-Next 2.8 bc43bce, hidden offline runs): GoldenEye
007, Mario Kart 64 and F-Zero X boot to gameplay with a scripted pad, react to
it (identical inputs give identical pictures), steer (by roll, now an option) and accelerate or
brake on the triggers; GoldenEye's crosshair lands within 0.005 of the screen
point the controller aims at (13 points inside its aim box, 4:3 and 16:9; one
16:9 point measured 0.012 where sky hid part of the crosshair). The aim box is
about the middle 49% of the 16:9 width and 60% of the height; pointing past it
turns Bond. In 3D, measured
disparities follow depth: Mario Kart 64 with `CONV` 60 puts the kart just in
front of the screen, the track behind it and the sky at the eyes' separation,
the HUD on the screen. Not yet seen in a headset.

## Nintendo 3DS games (Azahar)

The 3DS draws both eyes itself, so the same program shows them with no patch:
the prebuilt `azahar_libretro.dll` (GPL-2.0, the Citra successor; the
buildbot's Windows x86_64 build) with

```
--option citra_layout_option=single_screen --option citra_render_3d=side-by-side-full
--option citra_factor_3d=40 --option citra_resolution_factor=3
ACVR_PS1_SBS=1
```

`single_screen` keeps only the top (3D) screen. `side-by-side-full` puts the
left eye in the left half of the picture and the right eye in the right half,
but it keeps the base 5:3 picture size, so at factor 3 the 1200x720 frame holds
two eyes of 600x360 in the middle half of the rows, with black above and below.
That is 1.5 times the top screen's native 400x240 per eye, not 3 times: the
core fits both eyes into a frame sized for one screen. A higher
`citra_resolution_factor` sharpens the picture; its cost is untested.

The host cuts the black rows itself. It reads the frame size and the aspect the
core reports (5:3), works out the share of rows to cut (`.25` for 1200x720) and
the aspect of the screen, and does it again whenever the core sends new
geometry. `ACVR_PS1_LETTERBOX=<share>` overrides the cut. Sampling stays half a
texel inside each eye, so the cut edge and the other eye's edge do not bleed in.
Its log line is `Frame shape: ...`.

`citra_factor_3d` is the 3D slider (0 to 100). The recommended start is 40, not
100: on the 2.4 m curved theatre screen the picture spans about 55 degrees
against 12 to 17 for a real 3DS, so full-strength parallax is several times a
comfortable amount (a review's estimate, not yet judged in a headset). Raise it
per game once it has been looked at. The eye order was measured offline (see
Eye order below): the left half is the left eye, so no swap is needed. If far
scenery still comes towards you in a headset, set `ACVR_PS1_SWAP_EYES=1` (the
profile's `swap-eyes` view does it).

Starting it from the Hub: the catalog entries `3ds-star-fox-64-3d` and
`3ds-mario-kart-7` each have a Hub row (`hub/src/core/libretro`) that runs
`acvr-ps1 --core <core> --game <dump> --system <folder> --saves <folder> --profile <profile>`,
with `--view swap-eyes` for the diagnostic view. Nothing but the profile name is
in the repo. The Hub's Set-up page has a "Nintendo 3DS" card (under Consoles)
that fills the four settings; they are kept in the Hub's own private settings
(`user/hub-settings.toml`):

| Set-up page | Setting | What |
|---|---|---|
| "acvr-ps1 program" (Locate program) | `libretroHost` | the `acvr-ps1.exe` to run (a build that reads `--profile`) |
| "Nintendo 3DS core" (Choose) | `threedsCore` | Azahar's `azahar_libretro.dll` |
| "Your files" (Choose folder) | `threedsGameFolder` | your own decrypted dumps; each file is found by the game's title |
| "Profile folder (optional)" (Choose) | `libretroProfiles` | the profile folder; empty = `setups/retro-ps1-vr/profiles` found above the Hub |

The row appears once any of the first three is set, and says what is missing
until all are. Set-up's report lists each game as found, not right (and why) or
not found. The core's system and save folders are the Hub's own, under
`user/libretro/<id>`. Exit game here asks the host to save and quit through the
same stop file as PS1 and N64.

Azahar refuses a decrypted dump whose header it still reads as encrypted ("This
ROM is encrypted"). This repository carries no tool and no instructions for
that; follow Azahar's own documentation, and work on a private copy, never the
original.

Star Fox 64 3D (Type A controls) in `--controls pad`, from its own tutorial;
Azahar's RetroPad A, B, X, Y, L, R are the 3DS buttons of the same names, the
left stick is the Circle Pad:

```
ACVR_PAD_MAP=rtrig=a,ra=y,rgrip=x,lgrip=b,ltrig=l,rb=r,rleft=l,rright=r,lclick=start
```

| VR control | 3DS | Star Fox 64 3D |
|---|---|---|
| left stick | Circle Pad | steer (push up and boost for a somersault, up and brake for a U-turn) |
| right trigger | A | laser (hold to charge a lock-on shot) |
| right A | Y | bomb |
| right grip | X | boost |
| left grip | B | brake |
| left trigger, right stick left | L | bank left (twice: barrel roll) |
| right B, right stick right | R | bank right (twice: barrel roll) |
| left stick click | Start | pause (no right stick click alias: the right stick banks here) |

The game asks in its tutorial whether to turn on gyro aiming; the host sends no
motion, so answer no. Its "tap B to realign the gyro controls" prompt can stay
on screen for a tester who answered yes.

### Mario Kart 7

Profile `profiles/3ds-mario-kart-7.toml`, catalog id `3ds-mario-kart-7`, same
stereo settings as Star Fox (factor 40: the README has no Mario Kart number of
its own). The left stick is the Circle Pad and steers; racers steer with the
stick by default here, and roll or tilt steering is not offered for the 3DS (it
is an N64 setting and stays off). The map is the game's own controls (A
accelerate, B brake and reverse, R hop and drift, L item, X look behind, taken
from its manual as remembered, not checked against a copy of the game) put on
comfortable VR controls:

```
ACVR_PAD_MAP=rtrig=a,lgrip=b,rgrip=r,ltrig=l,ra=l,rb=x,lclick=start
```

| VR control | 3DS | Mario Kart 7 |
|---|---|---|
| left stick | Circle Pad | steer |
| right trigger | A | accelerate |
| left grip | B | brake, reverse |
| right grip | R | hop, hold to drift |
| left trigger, right A | L | use an item (either hand) |
| right B | X | look behind |
| left stick click | Start | pause (the left menu button is dead under ALVR) |

Not yet checked in a headset: the pad map, the comfort of factor 40 on a racer,
and the controls above. The only evidence so far is the offline eye-order run
below (Mario Kart 7 boots and draws per-depth disparity).

Results so far (Azahar 2127.0-alpha3, hidden offline runs, 3x): Star Fox 64
3D, Kid Icarus: Uprising and Mario Kart 7 boot and draw per-depth disparity
(Mario Kart 7's race 1 to 28 px across 24 levels; Star Fox's HUD at 0, its grid
14 to 24 px; the Kid Icarus measurement hit its search limit, so it says
nothing). In Star Fox's training course the controls sheet shows the effect of
the laser (a green bolt) and the bomb (a blast flash). Boost, brake, bank and
steering are mapped and the core accepted them, but the course moves between
frames, so there is no controlled before-and-after for them. The theatre proof
shows each eye cropped to fill the curved screen, rendered offline from a boot
logo and a training frame; the live headset path is not covered. Not yet seen
in a headset.

### Bottom screen as a touch panel (Kid Icarus: Uprising)

Many 3DS games need the touch screen: Kid Icarus: Uprising is aimed with the
stylus, and menus are tapped. With `ACVR_PS1_3DS_TOUCH=1` the top screen stays the
stereo theatre screen and the bottom screen is a second, smaller flat panel in
front of it and below, tilted to face you (like holding the 3DS). The right
controller's ray on the panel is the stylus: it is down while the right trigger
is held (`ACVR_PS1_3DS_STYLUS`, any VR control name of the pad map). A little
past the panel's edge counts as the edge, so a jittery hand does not lift the
stylus.

How it works, from the core's own behaviour (hidden offline runs, Mario Kart 7
and Kid Icarus: Uprising, Azahar 2127.0-alpha3):

- `citra_layout_option` is `default` (top over bottom), `single_screen`,
  `large_screen` or `side_by_side`; `citra_swap_screen` is `Top` or `Bottom`.
  With `citra_render_3d = side-by-side-full` and 3x, **`default` gives a
  1200x1440 frame in which each half is one eye's whole layout**: the top screen
  at 600x360 (x 0..600 and 600..1200, rows 360..720, so the same size per eye
  as the single-screen path) and **the bottom screen under it at 480x360, drawn
  twice** (x 60..540 and 660..1140, rows 720..1080), black elsewhere. The other
  layouts are worse for a panel: `large_screen` 1920x960 makes the bottom screen
  160x120 per eye, `side_by_side` 2160x720 puts it at 480x360 beside the top
  screen in each half, `single_screen` with `citra_swap_screen = Bottom` is the
  bottom screen alone (960x720, two copies) and loses the top screen.
- The core reads `RETRO_DEVICE_POINTER` every frame (`citra_enable_touch_touchscreen`,
  on by default) and takes a pressed pointer as the stylus. Its input
  descriptors also offer R3 as "Touch Screen Touch" and the right stick as a
  pointer (`citra_analog_function`); the host uses neither. The pointer
  position is a share of the whole frame, -32767..32767 from its top left;
  the core finds the bottom screen in its own layout (it is assumed to be the
  left copy: **not yet checked that a tap on it registers**, see below).

So the host cuts two rectangles out of the one frame, given as shares of its
width and height, `x, y, w, h` from the top left, in the profile:

| Setting | What |
|---|---|
| `ACVR_PS1_3DS_TOUCH` | 1: the touch panel. Needs a pad profile (`controls = "pad"`), the next two settings, and `ACVR_PS1_SBS=1` when the core draws two eyes |
| `ACVR_PS1_3DS_TOP` | the top screen, both eyes: `[0, 0.25, 1, 0.25]` at `default` and `side-by-side-full` |
| `ACVR_PS1_3DS_BOTTOM` | the bottom screen, once (the left copy): `[0.05, 0.5, 0.4, 0.25]`; also where the stylus is sent |
| `ACVR_PS1_3DS_POINTER` | where the core takes the bottom screen to be when it reads the stylus, default the bottom rectangle. An override for the stylus's position. The default is right for the crop, but taps do not land where aimed (see below); the mono layout's rectangle `[0.1, 0.5, 0.8, 0.5]` did not fix it |
| `ACVR_PS1_3DS_STYLUS` | the VR control that puts the stylus down, default `rtrig` |
| `ACVR_PS1_3DS_PANEL` | panel width, distance ahead, height against your eyes, metres: `0.8, 1, -0.55` (it tilts to face you) |

The screen's and the panel's aspect ratios come from those rectangles and the
core's frame size, so a different `citra_resolution_factor` needs no change.
Offline: the script line `touch <u> <v> [frames]` holds the stylus over the
panel (0..1 from its top left; `touch off` lifts it); with the setting on, each
snapshot also saves `<name>-top.png` and `<name>-panel.png` (a cross marks the
stylus: yellow over the panel, magenta down) and the log says how often the
core read the pointer. `--theatre-proof <prefix> --test-touch U V` renders both
eyes with the right hand aimed at the panel.

Profile `profiles/3ds-kid-icarus-uprising.toml` (not a Hub game yet: no catalog
entry, no Hub row). From the game's manual as remembered, not checked against a
copy of it:

```
ACVR_PAD_MAP=ltrig=l,ra=a,rb=b,lclick=start
```

| VR control | 3DS | Kid Icarus: Uprising |
|---|---|---|
| left stick | Circle Pad | move Pit (flick it to dash on the ground) |
| right controller's ray on the panel, right trigger held | stylus | aim the reticle; tap menus |
| left trigger | L | shoot (hold to keep firing) |
| right A, right B | A, B | menus and dialogs |
| left stick click | Start | pause (the left menu button is dead under ALVR) |

Roll steering does not apply. R, the D-pad, X and Y are not mapped (their uses were
not checked). Not checked in a headset: the panel's size and position, whether the
stylus lands where the ray points, aiming comfort, the map above.

Round 2 (hidden offline runs, corrected rectangles): the host's own `before-panel.png` is exactly the 480x360 bottom screen; a synthetic tap changed the game in all three runs (Mario Kart 7 left copy and right copy: Grand Prix menu to the 50cc page, which does not happen without a tap; Kid Icarus: first dialog to the next one), so the core takes the stylus from `RETRO_DEVICE_POINTER`. The tap aimed at "Time Trials" (panel 0.5, 0.35) opened Grand Prix instead, and the right copy did the same, so the core does not use the copy's own rectangle. The mono layout's rectangle `[0.1, 0.5, 0.8, 0.5]` (a tap at its 0.175 down is on Grand Prix) fits both, but round 3 tried it and it did **not** land: with `ACVR_PS1_3DS_POINTER=0.1,0.5,0.8,0.5` a tap aimed at Time Trials (0.5, 0.35) and one at Coin Runners (0.5, 0.73) both opened Grand Prix again. Five taps in all, at frame y 0.59, 0.68 and 0.87 and x 0.25, 0.5 and 0.75, all opened Grand Prix, so where the core puts the stylus is still unknown (it may clamp to the top button, or any tap may confirm the highlighted item). The stylus reaches the game and changes it; its position is not confirmed.

Earlier evidence (E2, hidden offline run): the layout facts above; the core
polls the pointer every frame and reads the stylus pressed on the frames the
script held it (10 pressed reads for a 10-frame tap, at the frame position the
mapping gives); the rectangles checked on a raw frame. **Not shown:** a tap on
the right spot changing something in the game. The run that was meant to show it
used rectangles that were not yet right (the probe's measurements had not been fed
back into it), so its taps landed on the seam between the two eyes.

### Eye order (offline measurement, 2026-10-25)

Evidence level: E2 (hidden offline runs; not seen in a headset). Tool:
`eyes.py` (a lane script, not in the repo): for each 24 px block of the left
half it finds the shift d, from -80 to +80 px, at which the right half matches
it. Sign convention: d greater than 0 means the right-eye picture is shifted
right relative to the left, which is uncrossed parallax, behind the screen
plane (far); d less than 0 is crossed, in front of the screen; 0 is the screen
plane. Far objects must come out positive. A block whose best match sits at
-80 or +80 is flagged at the limit (the sign is known, the size is not), and a
block near the picture's edge whose best sits at its clipped search end is left
out.

Run: the host with `--profile 3ds-star-fox-64-3d.toml` (factor 40, 3x, 1200x720
frames), `--headless --no-xr --mute`, 6000 frames, a snapshot every 600 frames
while a script pressed A and Start; Star Fox 64 3D and Mario Kart 7, 9 frames
each, one run each. The eyes differ in every frame (mean absolute difference 0.1
to 11 grey levels).

| Game | Textured blocks used | d greater than 0 | d less than 0 | d = 0 | Far shift | At the limit |
|---|---|---|---|---|---|---|
| Mario Kart 7 | 2619 | 2335 | 52 | 232 | per-frame medians +4.5 to +13 px (race frames: median +7, up to +11) | 18 blocks at -80 |
| Star Fox 64 3D | 1995 | 1281 | 393 | 321 | histogram peaks at +9 and +10 px | 2 blocks at -80 |

The HUD sits at 0 in both games (232 and 321 zero-shift blocks; in Mario Kart 7
the "8th" and the kart are at the same x in both halves of frame 4200, while the
banners and the arch at the end of the straight are shifted right). In Star Fox
two frames (1800 and 4800) have more crossed than uncrossed blocks (medians -1
and -3): in frame 4800 the near ring fills the picture, down to -37 px in front
of the screen, while the grid and the stars behind it are at +9 and +10. Both games
therefore put far scenery on the uncrossed side: **the eyes are not swapped, and
`ACVR_PS1_SWAP_EYES` stays off in the 3DS profile.** The 20 blocks at -80 are
near objects or false matches on a repeating texture; their size is unknown. Not
measured: other games, other frame sizes, and the effect in a headset.

## Next

The owner's headset verdict on stereo comfort and on the diorama; the Justifier
for SwanStation, so Die Hard Trilogy can be 3D as well.

The program is GPL-3.0-or-later. `retro_abi.h` declares the libretro core
interface from its public description and contains no libretro code.
