# Fresh geometry authoring API

Each artist owns one Python module. `from fresh import *`; `build(model_id, detail=0)` creates geometry and returns a dict of semantic anchors. All measurements are millimetres. Coordinates everywhere are `(forward, height, lateral)`; muzzle is positive forward. Internally Blender uses `(lateral, forward, height)` and export converts to glTF +Y up / -Z forward. The grip web is near `(0,0,0)`. New silhouettes and mesh geometry only; do not import old builders or meshes.

Available helpers (implemented by coordinator):

- `profile(name, points, depth, mat='body', bevel=1.0, lateral=0, parent=None)` closed 2D `(forward,height)` outline extruded in thickness; bevels edges. Use concave polygons safely, no holes.
- `box(name, loc, size, mat='body', bevel=1, parent=None)` loc FHL, size `(length,height,width)`.
- `cyl(name, loc, radius, depth, mat='accent', axis='f', vertices=24, bevel=.5, parent=None)` axis `f`, `h`, or `l`.
- `tube(name, loc, outer, inner, depth, mat='accent', axis='f', vertices=24, bevel=.3, parent=None)` genuine hollow tube.
- `sphere(name, loc, scale, mat='body', segments=24, rings=12, parent=None)` scale FHL radii.
- `ring(name, outer, inner, depth, mat='body', bevel=1, lateral=0, parent=None)` closed trigger guard from equal-count matching 2D loops.
- `rod(name, start, end, radius, mat='accent', vertices=12, parent=None)` round bar between FHL points.
- `screw(name, loc, radius=2.2, axis='l', mat='accent', parent=None)` countersunk screw with dark slot. Use on both sides if relevant.
- `node(name, loc=(0,0,0), parent=None)` semantic empty; parent arguments can be node name or object. Meshes default to current LOD.

Materials: body = satin neutral light graphite/silver (tintable); accent = medium steel; dark = dark rubber; glass = deep blue optical surface. All assigned by name. Avoid text/logos/serials. No texture files. No stock/template grip shared across every type: author own curved profiles. Use detail 1 for simpler LOD1 (omit screws/serrations/vents, fewer round segments, bevel 1 segment is automatic).

Return example: `{'muzzle': (180,40,0), 'sight_front': (165,62,0), 'sight_rear':(-30,62,0), 'pivot_trigger':(20,-3,0), 'length_mm':240}`. Create `node('pivot_trigger', ...)` before parenting trigger geometry to it. Motion nodes should have real geometry children; a slide has `node('slide_recoil',...)`, then parent its panels. Do not move static muzzle/sight/FX anchors under moving parts. Optional anchor keys e.g. `grip_two`, `button_start`. Controllers return `asset_kind:'controller'`. Function may use `math` and `bpy` directly for custom lofts; label custom geometry with existing material and current LOD (use `finish(obj,mat,bevel,parent)`).

Modeling target: each model must have convincing primary volumes, a distinct silhouette, rounded cross-sections, seated parts, true cutouts, intentional secondary details, ergonomic grip. #001 is quality reference only. Aim at <=8000 LOD0 triangles and <=2000 LOD1; coordinator audits and adjusts. No engine/Blender invocation by workers; coordinator owns sequential CPU export/render.
