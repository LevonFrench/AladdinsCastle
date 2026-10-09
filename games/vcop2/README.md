# Virtua Cop 2 (Sega, 1995) - VR

The real Virtua Cop 2 levels around you in full 6DoF, with your motion controller as the light gun. A second controller becomes player 2.

## Features

- True 3D: VC2VR rebuilds the 3D scene the game draws every frame and renders it through OpenXR.
- Laser sight and revolver model; akimbo / player 2 on the second controller.
- The game's cinematic zooms are mirrored.
- Menus and score screens on a floating screen you click with the laser.

## How to use

1. **Find my files**: point the Hub at your Virtua Cop 2 PC install (the folder with `PPJ2DD.EXE`), or let it search.
2. **Install**, then **Start in VR ▶**. Keep the game's disc image mounted (the music comes from it).
3. Face your monitor when you put the headset on: your first head pose becomes the camera. Squeeze the grip to recentre.

## Controls

| Action | Control |
|---|---|
| Fire | Trigger |
| Reload / Start | A / X |
| Back | B / Y |
| Zoom toggle | Flick the thumbstick up |
| Floating menu screen | Click the thumbstick |
| Recentre | Grip |
| Player 2 joins | A / X on the second controller |

## What it installs

- **VC2VR v1.0 beta** by NeuralF, https://github.com/NeuralF/Rea-Virtua-Cop-2-VR (MIT)

## Requirements

- Your own Virtua Cop 2 for Windows (1997).
- Windows x64, any OpenXR runtime (SteamVR, Meta Quest Link, Virtual Desktop).

## Troubleshooting

- **`...BIN\MOTCMN.BIN` not found:** the 1997 loader wants its data one level up. See the upstream README's `PROJECT` folder workaround.
- **Full screen doesn't work:** VC2VR needs windowed mode (it sets this for you).
- **Glitches with fog or transparency:** known upstream limits of the reconstruction.

## Credits

NeuralF (VC2VR), Sega AM2 (original game).
