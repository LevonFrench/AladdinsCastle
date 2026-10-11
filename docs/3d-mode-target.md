# 3D mode: what it must look like

Status: 2026-10-10, owner's target (D53). The reference is the first five seconds of the hotd2-vr demo video (https://www.youtube.com/watch?v=A7b83PHgRB8, The House of the Dead 2 title screen in a Quest 3). Every true-3D setup we build is judged against these three things, whatever the game or emulator.

## The three things

| # | Owner's words | What is on screen in the reference | What it requires |
|---|---|---|---|
| 1 | **The HUD stabilised** | The title logo and its text hang on a flat panel in one place in the world. When the head turns or tilts, the panel stays where it is: by 4.5 s the head has tilted down and the panel is sliding off the top of the view. It is not glued to the face and it does not swim with the 3D scene behind it. | Split the game's 2D layer (HUD, text, menus, logos) from its 3D scene. Draw the 2D layer on a panel fixed in the play space at a set distance, framed as the original screen framed it. |
| 2 | **The wide frame** | The street scene fills the whole view edge to edge with no window or border, far wider than the original 4:3 picture, and stays filled as the head looks around. | Draw the scene with the headset's own field of view per eye. Widen the game's own field of view and its culling so geometry exists out to the edges (hotd2-vr raises this game from 41 to 74 degrees vertical inside the game). |
| 3 | **The gun mapped in the world** | From 2.5 s a silver pistol, with the hand and sleeve holding it, sits where the controller is: full size, lit like the scene, in front of both the world and the HUD panel, moving 1:1 with the hand. | Draw our gun model at the controller inside the same 3D space as the scene, with real depth against it. Position from the grip pose, direction from the aim pose ([guns.md](guns.md) §2.3). |

## What this rules in and out

- **A stereo picture on a virtual screen is not 3D mode.** It gives depth but none of the three: the HUD is part of the picture, the frame has an edge, and a gun drawn in front of a screen is not in the world. It stays useful as a fallback tier ("3D theatre") and nothing more.
- **Head-locked rendering is not 3D mode.** If the game's picture, HUD included, moves with the head, thing 1 fails.
- The renderer therefore has to sit inside the emulator or engine, where 2D and 3D draws can be told apart and the camera can be replaced per eye.

## Acceptance (in the headset, any game)

1. Turn and tilt the head: the world stays put, the HUD panel stays put, and neither follows the head.
2. Look as far left, right, up and down as is comfortable: the scene has no edge and nothing pops out of existence at the old screen border.
3. Hold the gun up against a wall in the scene and against the HUD panel: it is in front of both, at a believable size, and does not lag the hand.
4. Shots land where the gun points, across the whole original screen area.
5. With the game's camera moving (a rail shooter's cart), the HUD panel and the gun stay steady relative to the player.

## Per-game notes

- Where the game's camera shakes on purpose (Resident Evil: The Darkside Chronicles is the known case), offer camera stabilising as a comfort option: smooth the game camera's rotation before it reaches the headset.
- Where widening the field of view is not possible yet, say so on the game's page; do not ship a windowed view and call it 3D mode.
