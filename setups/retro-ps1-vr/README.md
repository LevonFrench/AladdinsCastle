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
| Left menu button (Index: left stick click) | Pad Start |
| Home key | Put the screen in front of you again |

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
| `ACVR_PS1_AIM` | 1,0,1,0 | per-game aim trim: x scale, x shift, y scale, y shift around the centre, measured against where the game registers the shot |
| `ACVR_PS1_HIDE_FLASH` | 1 | keep the games' per-shot white flash frames out of the headset (the game still draws them and reads the gun). Only white frames while the trigger is held or within 20 frames after it count, so menus and fades always show |
| `ACVR_PS1_CHEAT_FILE`, `ACVR_PS1_CHEATS` | none | a DuckStation-format .cht file and the `;`-separated entries to enable |
| `ACVR_PS1_GUN_MODEL`, `ACVR_PS1_GUN_META` | none | the gun's `.glb` (`assets/guns`) and `.toml` (`data/guns`) |

The screen is placed level, straight ahead of where you look when the first
picture arrives; Home places it again.

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

`profiles.json` lists, per catalog game, which gun and port it uses.
`--option` sets a core option, such as
`--option beetle_psx_internal_resolution=2x`. The program writes every
option the core offers, with the value in use, to `core-options.txt` in the
saves folder.

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
| `wheel` | Rolled like a steering wheel: analog X, linear from a 3 degree dead zone to full lock at `ACVR_PAD_WHEEL_DEG` (75) | Mario Kart 64, F-Zero X |
| `aim` | While the right grip is held (`ACVR_PAD_AIM_HOLD`, or `always`), where it points on the screen sets the analog stick, with the game's aim button held (`ACVR_PAD_AIM_BUTTON`, N64 R), so the game's own crosshair follows it | GoldenEye 007 |
| `pad` | Buttons only; the left stick is the analog stick | anything else |

`ACVR_PAD_MAP` maps VR controls to RetroPad buttons, for example
`rtrig=b,ltrig=rs-down,rgrip=r,lgrip=l2` (F-Zero X: right trigger accelerates,
left trigger brakes on C-down, the grips lean). VR names: `rtrig ltrig rgrip
lgrip ra rb lx ly lmenu rclick lclick rup rdown rleft rright`; RetroPad names:
`b y select start up down left right a x l r l2 r2 l3 r3` and `rs-up rs-down
rs-left rs-right` (the right stick pushed fully). `ACVR_PAD_AIM` is the aim
trim `x scale, x shift, y scale, y shift` from the screen point to the stick,
measured per game against its own crosshair. The game's Rumble Pak
(`--option mupen64plus-pak1=rumble`) pulses the right controller. Saves
(EEPROM, SRAM, controller pak) are the core's save memory, kept in the saves
folder like the PlayStation memory card.

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

`ACVR_GL_TRACE=<first frame>,<frames>` (with `ACVR_GL_TRACE_DIR`) logs what a
core draws and saves its vertex shaders, to study another core.

Results so far (Mupen64Plus-Next 2.8 bc43bce, hidden offline runs): GoldenEye
007, Mario Kart 64 and F-Zero X boot to gameplay with a scripted pad, react to
it (identical inputs give identical pictures), steer by roll and accelerate or
brake on the triggers; GoldenEye's crosshair lands within 0.005 of the screen
point the controller aims at (13 points inside its aim box, 4:3 and 16:9; one
16:9 point measured 0.012 where sky hid part of the crosshair). The aim box is
about the middle 49% of the 16:9 width and 60% of the height; pointing past it
turns Bond. In 3D, measured
disparities follow depth: Mario Kart 64 with `CONV` 60 puts the kart just in
front of the screen, the track behind it and the sky at the eyes' separation,
the HUD on the screen. Not yet seen in a headset.

## Next

The owner's headset verdict on stereo comfort and on the diorama; the Justifier
for SwanStation, so Die Hard Trilogy can be 3D as well.

The program is GPL-3.0-or-later. `retro_abi.h` declares the libretro core
interface from its public description and contains no libretro code.
