# Original gun assets

Seven tier 1 source designs are built from primitives. No imported, extracted,
traced or third-party meshes, logos or markings are used. These are toy-like
peripherals with our own dimensions, not real firearms or branded replicas.
Built models and preview/callout assets are CC0-1.0; scripts are GPL-3.0-only.

## Rebuild

Ask the owner before invoking Blender, including headless/CPU use. The source
targets the locally installed Blender 5.2, with factory startup, no add-ons,
sequential processes, four CPU threads and Cycles explicitly set to CPU.
The approval flag records an existing approval; it does not grant one.

```powershell
python J:/projects/games/aladdinscastle/.local/worktrees/guns-controls/assets/guns/build.py --blender 'C:/Program Files/Blender Foundation/Blender 5.2/blender.exe' --owner-approved-cpu-batch
```

Or, after approval, build an individual script with `--background`,
`--factory-startup`, `--python-exit-code 1`, `--threads 4`, `--python <script>`,
then `-- --out <absolute-output.glb> --preview-dir <absolute-preview-directory>`.
Use `--samples 16` (default). `build.py --model <id>` selects a subset.
No application installation, downloads, GPU renderer, editor/MCP, or game files
are involved. The builder exports the neutral model before applying its p1 tint
for previews. Failures produce a nonzero process exit code.

## Hierarchy and coordinates

glTF 2.0 binary, metres, +Y up, -Z bore. One scene/root, `grip`, is identity at
the web of the holding hand. On the mounted design it is the right rear handle;
`grip_two` is the left rear handle. `LOD0` and `LOD1` each own all their meshes;
the runtime must select exactly one. Budgets: 8,000/2,000 triangles. Four used
materials: neutral `body`/`accent`, fixed `dark`/`glass`; no textures.

| Node | Meaning |
|---|---|
| `grip` | Controller grip anchor, root |
| `muzzle`, `fx_muzzle`, `fx_laser` | Static bore/effect anchors; never inherit recoil |
| `sight_front`, `sight_rear` | Static aiming references |
| `pivot_trigger` | Trigger hinge with visible child mesh |
| `slide_recoil` | Moving slide, 15 mm backwards along +Z |
| `limit_recoil` | Static slide end stop; slide extras also record 0.015 m |
| `visual_kick` | Visible-body recoil, separate from the ballistic ray |
| `button_<id>` | One named motion anchor per metadata button |
| `pivot_yaw`, `pivot_pitch` | Nested mounted yoke/receiver hinges |
| `grip_two` | Mounted left rear grip |

Canonical animated names appear in LOD0; LOD1 equivalents have `_lod1` suffixes.
Both carry `extras.semantic_node`. Metadata `[motion.<id>]` includes `lod_nodes`,
unit axis, increasing range in metres/radians, and its input/event drive.
The lead accepted this convention for the v0.1 controls contract.

## Previews and callouts

The CPU batch writes 512×512 `<id>.png` (three-quarter, p1 tint) and
`<id>-front.png` (left-side elevation for part labels). `<id>.json` has version,
model, image size and `views.front/threequarter`: each supplies an image basename
and every semantic node's normalized `[x,y]` position (origin top left).
These are camera projections computed from the actual model, not guessed labels.
`sheet.png` is a 4×2 contact sheet in model order:
generic, slide, twin, slim, dpad, chunky, mounted. It contains no vendor art.

## Validate and add a model

```powershell
python J:/projects/games/aladdinscastle/.local/worktrees/guns-controls/tools/check_gun_assets.py
python J:/projects/games/aladdinscastle/.local/worktrees/guns-controls/tools/test_check_gun_assets.py
python J:/projects/games/aladdinscastle/.local/worktrees/guns-controls/tools/gun_models.py
```

The checker reads real vertex/index buffers and node transforms to measure each
LOD; declared accessor bounds cannot fool it. It checks file/chunk/buffer bounds,
scene roots, unique names, bore direction, triangle/material limits, buttons,
motion aliases/limits, both PNG dimensions and every required callout anchor.
`--source-only` is available before Blender approval and explicitly reports
that it provides no built-asset acceptance. The default check fails on missing
outputs; never use source-only mode as a replacement for the final CI gate.

Add a descriptive id and top-level parameter script, original primitives in
`src/common.py`, metadata/motion in `data/guns/`, and include the id in the batch.
Rebuild, run both tools, inspect the previews and report budgets/bounds. Structural
reproducibility means the same nodes, bounds and budgets, not byte equality.
Shape-source review flags remain until manuals/photos provide evidence.
