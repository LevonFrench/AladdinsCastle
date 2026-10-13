# Time Crisis (Namco, 1995) - VR

The intended VR setup renders the original arcade world in stereo, with a tracked pistol and a controller button for the cover pedal.

**Status:** the owned namco22-vr Hub setup is planned and cannot yet be installed or launched from the Hub. Real-game left/right desktop captures have been produced, but they do not prove a headset connection, tracked aim or a playable Hub VR route. The newer native adapter remains experimental. The Hub can offer "Play in MAME" as a flat route when its emulator and media requirements are met.

## Planned VR features

- True 3D: the original arcade game, rebuilt by the namco22-decompile project, rendered per eye by libacvr.
- Tracked pistol with optional laser and adjustable gun angle; aim is projected through the arcade camera, so off-centre shots land where you point.
- Cover pedal on a button: hold either grip to stay out and shoot, release it to take cover and reload (inverted and toggle modes in settings).
- Left- or right-handed play; pulling a trigger picks that gun hand.

## Current launch options

Use the available flat MAME route in the Hub. There is no supported **Install → Start in VR** sequence for the owned setup yet. Desktop stereo diagnostics are separate experiments; they do not enable the planned Hub route.

## Planned VR controls

| Action | Control |
|---|---|
| Fire / pick gun hand | Either trigger |
| Out of cover (pedal down) | Hold either grip |
| Take cover / reload | Release the grip |
| Insert credits | A right (X left in left-handed mode) |
| Laser on/off | B right (Y left) |
| Pause / options | Left menu button |
| Recenter | Hold both grips |

## Planned components

- **namco22-decompile** by spacestate1 and contributors, https://github.com/spacestate1/namco22-decompile (MIT)
- **AladdinsCastle namco22-vr** adapter with libacvr; see each component's included licence.

## Intended VR requirements

- Your own `timecris` set: Time Crisis World TS2 Ver.B (MAME 0.271+), matched by hash.
- PCVR through SteamVR with a SteamVR headset.

## Credits

spacestate1 and contributors (namco22-decompile), DR-89 (Time Crisis VR, whose aim-projection and VR host work is prior art for this setup), Namco (original game).
