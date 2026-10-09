# Time Crisis (Namco, 1995) - VR

Stand inside the original Time Crisis: the arcade game's own 3D world in stereo around you, a tracked pistol in your hand, and the cover pedal on a button.

**Status:** planned for milestone M2 (AladdinsCastle's own true-3D setup on namco22-decompile + libacvr). Until then the Hub offers "Play in MAME" (flat window).

## Features

- True 3D: the original arcade game, rebuilt by the namco22-decompile project, rendered per eye by libacvr.
- Tracked pistol with optional laser and adjustable gun angle; aim is projected through the arcade camera, so off-centre shots land where you point.
- Cover pedal on a button: hold either grip to stay out and shoot, release it to take cover and reload (inverted and toggle modes in settings).
- Left- or right-handed play; pulling a trigger picks that gun hand.

## How to use

1. **Find my files**: point the Hub at your `timecris` set, or let it search.
2. **Install**, then **Start in VR ▶** (or start it from your SteamVR library).
3. In game: **A** (right) inserts credits, then either trigger starts.
4. Use **Map controls** (Control Mapping mode) to adjust the gun angle, laser and cover mode while you watch the pistol move 1:1.

## Controls

| Action | Control |
|---|---|
| Fire / pick gun hand | Either trigger |
| Out of cover (pedal down) | Hold either grip |
| Take cover / reload | Release the grip |
| Insert credits | A right (X left in left-handed mode) |
| Laser on/off | B right (Y left) |
| Pause / options | Left menu button |
| Recenter | Hold both grips |

## What it installs

- **namco22-decompile** by spacestate1 and contributors, https://github.com/spacestate1/namco22-decompile (MIT)
- **AladdinsCastle namco22-vr** host with libacvr (MIT)

## Requirements

- Your own `timecris` set: Time Crisis World TS2 Ver.B (MAME 0.271+), matched by hash.
- PCVR through SteamVR (Steam Frame streaming, Quest 3 via ALVR / Link / Virtual Desktop / Steam Link).

## Credits

spacestate1 and contributors (namco22-decompile), DR-89 (Time Crisis VR, whose aim-projection and VR host work is prior art for this setup), Namco (original game).
