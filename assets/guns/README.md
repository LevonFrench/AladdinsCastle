# Original gun assets

Seven tier 1 source designs have independently authored closed side outlines
(25–50 points in millimetres), symmetric extrusion and rounded edges. Shared
code handles geometry and export; each model script owns its silhouette.
No imported, extracted,
traced or third-party meshes, logos or markings are used. These are toy-like
peripherals with our own dimensions, not real firearms or branded replicas.
Built models and preview/callout assets are CC0-1.0; scripts are GPL-3.0-only.

## Rebuild

Ask the owner before invoking Blender, including headless/CPU use. The source
targets Blender 5.2, with factory startup, no add-ons,
sequential processes, four CPU threads and Cycles explicitly set to CPU.
The approval flag records an existing approval; it does not grant one.

```powershell
python assets/guns/build.py --blender '<path-to-existing-blender-executable>' --owner-approved-cpu-batch
```

Or, after approval, build an individual script with `--background`,
`--factory-startup`, `--python-exit-code 1`, `--threads 4`, `--python <script>`,
then `-- --out <absolute-output.glb> --preview-dir <absolute-preview-directory>`.
Use `--samples 64` (default and required for this art pass).
`build.py --model <id>` selects a subset.
No application installation, downloads, GPU renderer, editor/MCP, or game files
are involved. The builder exports the neutral model before multiplying its
linear body/accent base factors by the p1 palette tint for previews, matching
native rendering. Fixed dark/glass RGBA stays unchanged. Failures produce a
nonzero process exit code.

## Hierarchy and coordinates

glTF 2.0 binary, metres, +Y up, -Z bore. One scene/root, `grip`, is identity at
the web of the holding hand. Runtime places this point at the OpenXR grip
position and orients the model bore using AIM orientation plus the gun angle.
On the mounted design it is the right rear handle;
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

Blowback moves the upper slide, its visible sights and lens. Frame, hand grip,
guard and cable boss stay stationary. The twin's whole-body visual
kick carries all visible furniture while its aim/reference anchors stay fixed.
Visible sight parts are `front_sight_geometry`/`rear_sight_geometry`; the static
`sight_front`/`sight_rear` names and transforms stay reserved for reference nodes.
Both anchors share one height parallel to the bore. The front is a 3 mm blade;
the rear is a 6 mm open notch with level top edges. Bases retain their original
seated positions; a taller sight bridges any roof-height difference. Both LODs
retain the complete notch profile. Mounted yaw/pitch intent remains unchanged.

Canonical animated names appear in LOD0; LOD1 equivalents have `_lod1` suffixes.
Both carry `extras.semantic_node`. Metadata `[motion.<id>]` includes `lod_nodes`,
unit axis, increasing range in metres/radians, and its input/event drive.
The lead accepted this convention for the v0.1 controls contract.

## Previews and callouts

The CPU batch writes 512×512 `<id>.png` (three-quarter, p1 tint),
`<id>-front.png` (left-side elevation for part labels) and `<id>-held.png`
(behind and slightly above in the model's aim frame). CPU Cycles uses 64 samples
and OpenImageDenoise with GPU denoising disabled, Standard transform, no look,
gamma 1 and exposure 0. The emission-only preview shader matches
`runtime-lighting.json`: normalized camera-space light `(0.36, 0.70, 0.62)` and
factor `0.35 + 0.65 * max(dot(normal_view, light), 0)`, applied to the exported
linear base factors and linear P1 tint. There are no physical lamps, specular,
shadows or tone mapping. The PNG encodes linear colour to sRGB. This is the R2
runtime lighting contract at each preview camera angle, not scene matching or
evidence of an accepted headset render. Product alpha is retained.
`<id>.json` has version,
model, image size and `views.front/threequarter/held`: each supplies an image basename
and every semantic node's normalized `[x,y]` position (origin top left).
These are camera projections computed from the actual model, not guessed labels.
`sight_reviews.LOD0/LOD1` names two additional 512-pixel views looking straight
through the rear notch along the bore; inspect both before acceptance.
`sheet.png` is a 4×2 contact sheet in model order:
generic, slide, twin, slim, dpad, chunky, mounted. `silhouette-sheet.png` combines
flat black side projections on white at a shared 0.82 m square camera scale.
Individual silhouettes are `<id>-silhouette.png`. They contain no vendor art.

## Validate and add a model

```powershell
python tools/check_gun_assets.py
python tools/test_check_gun_assets.py
python tools/gun_models.py
```

The checker reads real vertex/index buffers and node transforms to measure each
LOD; declared accessor bounds cannot fool it. It checks file/chunk/buffer bounds,
scene roots, unique names, bore direction, triangle/material limits, buttons,
motion aliases/limits, all three PNG dimensions and every required callout anchor.
Tier 1 also requires finite unit vertex normals and actual exported notch/blade
geometry on both LODs. Ray tests check an open notch, both rear cheeks and base,
a narrow front blade and a clear path between the sights. These tests use world
triangles, not only claimed anchors or accessor bounds. Preview receipts must
match the runtime-lighting contract and include both sight-through images.
For each LOD separately, every mesh instance's world bounds must touch or overlap
another's, measured from referenced vertices. This is an AABB contact check;
visual inspection still checks that actual surfaces meet.
Actual-asset checks also reject unsupported drives, non-integer/out-of-range
durations, repeated motion targets and driven ancestors of static references
(including either LOD). Native static names reserve `grip`, `grip_two`, `muzzle`
and every `sight_`/`fx_` prefix. Material RGBA and decoded vertex RGB/RGBA must be
finite in 0..1; normalized unsigned byte/short vertex colors are supported.
Distinct motion targets may share a logical drive. The checker deliberately
keeps the authored tier's two explicit LOD aliases and four used material roles;
it supplements the native decoder and does not replace its acceptance gate.
`--source-only` is available before Blender approval and explicitly reports
that it provides no built-asset acceptance. The default check fails on missing
outputs; never use source-only mode as a replacement for the final CI gate.

Add a descriptive id, parameters and an original `make_design` shape function
in its model script, metadata/motion in `data/guns/`, and include the id in the batch.
Rebuild, run both tools, inspect the previews and report budgets/bounds. Structural
reproducibility means the same nodes, bounds and budgets, not byte equality.
Shape-source review flags remain until manuals/photos provide evidence.
