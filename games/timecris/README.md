# Time Crisis (Namco, 1995) - VR

Stand inside the original Time Crisis: the arcade game's own 3D world in stereo around you, a tracked pistol in your hand, and real ducking to take cover.

## Features

- True 3D: the original arcade game rebuilt from the namco22-decompile project and rendered per eye.
- Tracked pistol with optional laser; adjustable gun angle.
- Cover pedal: duck for real (physical cover) or hold a grip button.
- Left- or right-handed play; trigger picks the gun hand.
- Quest 3 standalone or PCVR.

## How to use

1. **Find my files**: point the Hub at your `timecris.zip`, or let it search.
2. **Install**, then **Start in VR ▶** (or start it from your SteamVR library).
3. In game: **A** (right) inserts credits, then either trigger starts.
4. Stand upright and press **X** (left) once to recenter and set your standing height for ducking.

For Quest standalone, the headset needs **developer mode** and USB debugging. The Hub walks you through it.

## Controls

| Action | Control |
|---|---|
| Fire / pick gun hand | Either trigger |
| Take cover | Duck (physical mode) or hold either grip (grip mode) |
| Insert credits | A right (X left in left-handed mode) |
| Laser on/off | B right (Y left) |
| Pause / options | Left menu button |
| Recenter + set height | X left (A right) |

## What it installs

- **Time Crisis VR** by DR-89, https://github.com/DR-89/time-crisis-vr (MIT port code), built on
- **namco22-decompile** by spacestate1, https://github.com/spacestate1/namco22-decompile (MIT)

The Hub doesn't download DR-89's release packages because they ship game files. Use a ROM-free build plus your own `timecris.zip`.

## Requirements

- Your own `timecris.zip`: Time Crisis World TS2 Ver.B, MAME 0.271+ set.
- PCVR: an OpenXR runtime with OpenGL support (SteamVR, Meta Quest Link). Quest 3 via Link, Air Link or Virtual Desktop.
- Quest standalone: developer mode + USB cable.

## Troubleshooting

- **Aim is off by a fixed angle:** options → gun angle (try -45 if it points about 45° too high).
- **Ducking triggers too easily:** recenter while standing fully upright.
- **No picture in the headset (PCVR):** make sure the active OpenXR runtime supports OpenGL; try `Play SteamVR.cmd`.

## Credits

DR-89 (VR port), spacestate1 and contributors (namco22-decompile), Namco (original game).
