# Guns: art pass after the first build

Review of the first Blender batch (seven models, fourteen previews, checker at 0 errors). The pipeline works end to end: scripts build, named nodes and both detail levels export, previews and callout files come out, the checker reads the real files. That part is done. **The shapes are not.** This page is the art direction for the second pass. It belongs to lane 3 ([3-guns-controls.md](3-guns-controls.md)).

## What the first renders show

- The six pistols are one blockout: a long box on a short box, a slab for a grip, a wire rectangle for a trigger guard. `generic-pistol` and `arc-pistol-slide` cannot be told apart. The others differ by colour, one cylinder, or a few dots.
- **The grip leans the wrong way.** Its bottom sits toward the muzzle. A pistol grip rakes back: the bottom is behind the top.
- Parts float: both sight blocks hover above the body, the rear block hangs off the back, the trigger touches nothing, three pegs stick out of the back of the grip, and the mounted gun's rear handle hangs off a thin bar.
- Nothing yet evokes what it is meant to resemble: no slide on the slide pistol, no rear d-pad visible on the d-pad pistol, nothing chunky or revolver-like about the chunky revolver.
- The previews are overexposed (clipped highlights, washed colour) and noisy.

## How to model them: the side silhouette first

A gun is recognised by its outline from the side. Build every model from one, not from boxes.

1. **Body outline.** Each model's script defines its body as a closed 2D outline in millimetres, 25 to 50 points, as seen from the left side with the muzzle pointing left. Extrude it symmetrically to the model's thickness and round the edges (2 to 4 mm). The outline is the model's own data; no shared function with `if model_id` branches.
2. **Separate outlines** for the parts that move or contrast: the slide or barrel shroud, the grip panels (slightly proud of the frame), the trigger (a curved blade, pivoting at its top), the trigger guard (a closed loop with real thickness, part of the frame outline).
3. **Everything touches.** Sights sit on the top surface. Buttons are seated in the body, standing 1 to 2 mm proud. No part may be separated from the rest by a gap. Add this to the checker: every mesh part's bounds must overlap or touch another's.
4. **Grip:** raked back 12 to 18 degrees, thicker than the barrel, rounded front and back, long enough for a hand (85 to 100 mm).
5. **Cross-section:** the top of a slide is flatter, the frame below it narrower; a barrel shroud can be octagonal or round. Two or three thickness steps are enough to stop it reading as a slab.
6. Keep the budgets (8,000 and 2,000 triangles) and the named nodes exactly as they are now.

## Per model: what must be recognisable

Lookalike rule unchanged ([guns.md](../../guns.md) §2.2): evoke the type, keep our own proportions, no logos or markings.

| Model | The silhouette to hit |
|---|---|
| `generic-pistol` | A plain compact pistol: short barrel, squared trigger guard, plain grip. Deliberately the least detailed. |
| `arc-pistol-slide` | Full-size service-pistol outline with a **visibly separate slide** on top: a seam along the side, serrations at the rear, the slide overhanging the frame at the back. Rounded, thick arcade plastic. A cable boss under the grip. |
| `arc-pistol-twin` | Stocky and tall: a deep squared barrel block, a **moulded cylinder bulge** in the middle like a revolver's, a wide trigger guard, short grip. |
| `con-pistol-slim` | Long and thin: a slim rectangular barrel with a **round lens** at the muzzle, a raised sight rail along the top, one oval button on each side just under the barrel ahead of the trigger guard, a slim raked grip with the cable leaving its base. |
| `con-pistol-dpad` | Shorter and rounder than the slim one: a **small d-pad on the back face** where a hammer would be, two buttons above the trigger guard, one at the bottom of the grip, a rounded hump over the rear. |
| `con-revolver-chunky` | Short, thick, blocky: a heavy squared barrel shroud, a fixed cylinder bulge, an oversized trigger guard, a Start button on the left side, a stubby grip. |
| `mnt-mg-heavy` | A fat perforated barrel shroud on a **U-shaped yoke** that grips the body from below at both sides, two rear handles (or one rear grip and a top carry handle) joined solidly to the body, a thumb button between them. |

## Previews

- Fix the import in `src/sheet.py` so `sheet.png` is produced.
- Lower the exposure until no highlight clips; use a neutral view transform; 64 samples with denoising on the CPU.
- Three views per model: side (muzzle left), three-quarter, and **as held**: from behind and slightly above, the way the player sees it in the headset.
- Add a **silhouette sheet**: all models as flat black shapes on white, side view, same scale. If two cannot be told apart at a glance, the pass is not finished.

## Done when

- The seven models rebuilt from side outlines, grip raked back, nothing floating, checker at 0 errors including the new touch check.
- `sheet.png` and the silhouette sheet committed with the models, previews and callout files.
- An outside look at the sheets says each model reads as its type and no two are confused.
