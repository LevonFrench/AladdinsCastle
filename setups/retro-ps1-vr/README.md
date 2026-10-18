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
| Right A | GunCon A (Justifier: its start button) |
| Right B | GunCon B |
| Hold left Y for 3 s (Index: left B) | Quit, saving the memory card |
| Home key | Put the screen in front of you again |

Each trigger pull gives a short vibration. An orange dot drawn into the
picture shows where the gun points. Escape on the PC also quits cleanly. A
launcher or the Hub can quit it by creating the file named in
`ACVR_PS1_STOP_FILE`.

## Theatre screen

The picture is scaled up by a whole number with sharp pixels to about 1440
lines before the headset gets it, so sprites stay crisp. The screen is
`ACVR_PS1_SCREEN_W` metres wide (default 2.4) at `ACVR_XR_DISTANCE` metres
(default 2.5), in front of where you look when the first picture arrives.

## Saves

Memory card 1 is the core's save memory. The program loads it from
`<saves>\<disc name>.srm` at start, writes it every 10 seconds when it has
changed and again on quit, always through a temporary file and a rename.

## Build

```
powershell -File build-host.ps1 -OpenXRInclude <folder with openxr\openxr.h> -OutputDirectory <private folder>
```

It needs MSYS2 MINGW64 `g++`. The OpenXR presenter is shared with the
Model 2 viewer (`..\mame-model2-vr\presenter`). Keep `openxr_loader.dll`
beside the program. Without a VR runtime the program still runs: the
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
720 trigger 4       # hold the trigger 4 frames
900 a 6             # also: b, start, select, offscreen N, reload
1200 snap title     # save the picture as title.png
```

In these runs the core draws its own crosshair, which shows where the game
believes the gun points. `--no-xr --mute --test-aim U V --snapshots <folder>`
runs at the game's own speed without a VR runtime and, on quit, reads back
from the GPU the exact picture the headset would get, aim dot included.

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

## Next

Real stereo for the polygon games needs a hardware-rendering core that
keeps each vertex's depth (PGXP). That is a patched build of a GPL core;
see the PS1 plan in the project notes.

The program is GPL-3.0-or-later. `retro_abi.h` declares the libretro core
interface from its public description and contains no libretro code.
