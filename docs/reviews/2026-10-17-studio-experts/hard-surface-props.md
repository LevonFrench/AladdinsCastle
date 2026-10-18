# Hard-surface props review: AC VR Hub arcade scene

Reviewer: hard-surface props (Blender hard-surface lens, applied to procedural Three.js 0.160). Report only; no site files edited.

## Scope and limits

- Read: BRIEF.md, `scenario-blender-hard-surface/SKILL.md`, `arcade.js` lines 56-100 and 580-1063, `docs/guns.md` section 2.2, captures `stop0` to `stop7` (the `-older` captures ignored).
- Labels: **[O]** observed in source or capture, **[I]** inferred from source maths, **[H]** hypothesis. No GPU test, no live page.
- Not judged from captures: ticket wheel, hoops, token changer detail, motion controller (stop1 caught the gun phase of the 7 s cycle). Those are source-only.

## Lens translation (what carries over)

Blender's stack (boolean cutters, Bevel modifier, Weighted Normal, subD support loops) does not exist at runtime here. Cheap Three.js equivalents:

| Blender technique | Three.js equivalent |
|---|---|
| Micro bevel catching a highlight on every edge | `RoundedBoxGeometry` (`three/addons/geometries/RoundedBoxGeometry.js`), segments 2, radius 1 to 2 cm on cabinets and 3 to 5 mm on guns (before the x2.5 display scale). Its smooth corner normals also stand in for Weighted Normal; flat faces stay flat. |
| Boolean cutters / holes | `THREE.Shape` with `shape.holes` extruded by `ExtrudeGeometry` (trigger guard, slots). |
| Silhouette blockout from a side profile | One `Shape` polygon per object, `ExtrudeGeometry({ depth, bevelEnabled, bevelSize: 0.003, bevelSegments: 2 })`. This is the biggest win for pistols and cabinets. |
| Revolved parts (grips, coin rims) | `LatheGeometry` with a 6 to 8 point profile. |
| Panel seams, edge wear | A shared 64 px canvas map, not geometry. |
| Edge highlight on glass | `LineSegments(EdgesGeometry(box))`. |
| Instancing a kit | `BufferGeometryUtils.mergeGeometries` per material, shared materials. |

Bevel size rule of thumb [I]: at the 3 to 4 m viewing distances in the captures, anything under about 1 cm is sub-pixel. The 6 mm Blender "micro bevel" would vanish, so go bigger than a render bevel would.

## Findings, ranked

### 1. HIGH. The stop-2 gun blows out to a white T-shape [O/I]
- `stop2-gun.jpg`: the one gun is a flat, bloomed white bar with a vertical stub; barrel, grip and guard are not distinguishable, and the glare spills onto the neighbouring cabinets.
- Cause [I]: `gunModel` line 877 gives the body an emissive of 0.12 on top of an albedo of `#f2f2f5` (`shell`, line 1014). Bloom is `UnrealBloomPass(..., 0.7, 0.5, 0.9)` (line 1071), so a lit near-white clips past the 0.9 threshold. The brief's whole point for this stop is "the right gun in your hand", and one of the five models cannot be read.
- Remedy: cap light bodies at about `#aeb4c2`, set emissive to 0 on `main` (keep it only on `trim`), and give the shell a dark grip and fore-end for value contrast. This is the same class of problem for `slide` (`#e8e8ee` trim, line 1014) and `revolver` (`#c9c9d2`). Optionally set `bloom.threshold` higher and rely on emissive materials only.

### 2. HIGH. Guns are stacked boxes; the side profile that identifies a pistol is missing, and the guard floats [O/I]
- A pistol is recognised from its side silhouette. `gunModel` (lines 874-931) builds it from separate boxes, so it reads as a "toy block" from the front and back during turntable rotation (`pivot.rotation.y = t * 0.45`, line 1030).
- Trigger guard floats [I, consistent with `stop5-wii.jpg`, where a detached arc hangs under the barrel]: the guard ring centre is `y = 0.012` (line 884) but frame bottoms are 0.02 to 0.0325 (`slide` frame centre 0.045 with height 0.035, lines 889, 910, 919). A 0.8 to 2 cm gap, times 2.5 on display, shows daylight between frame and guard.
- Grip rake looks inverted [I, from rotation maths, not verified in a capture]: `grip()` line 881 uses `rotation.x = +0.3 to +0.45`. With forward = -z, a positive rotation about x moves the top of the box to +z, so the grip's bottom leans toward the muzzle. Real pistol butts sit behind the trigger. Flip the sign, or check against a close frame.
- Revolver drum (line 902) is `CylinderGeometry(..., 6)`: a hexagonal prism reads as a nut. Use 12 radial segments plus six thin flute boxes, or six small cylinders on a ring.
- Remedy: build `slide`, `twin`, `revolver` and `dpad` frames as one extruded side-profile `Shape` (grip, frame, guard as a hole), depth about 0.03, bevel 0.003, drawn as an original outline so it stays a lookalike. Keep slide, drum and hammer as separate meshes so `animateShot` (lines 934-946) still works. About 150 triangles per gun.

### 3. MEDIUM-HIGH. Gun against a lit screen loses contrast at the console stops [O]
- `stop6-ps2.jpg`: the orange pistol sits directly in front of the pink sunset screen and its barrel merges with it. `stop5-wii.jpg`: the white gun reads, but the console behind it is dark and tiny.
- Cause: `gunOnTurntable(gun, x - 0.85, 1.35, z, ...)` (line 1060) places the gun on the camera-to-screen axis.
- Remedy: shift the gun about 0.4 m off the screen axis, or drop a dark backdrop plane behind it, or tone the kiosk screen texture lower on that side. Give guns a thin dark rim shell (a back-faced, slightly scaled clone, the cheap outline) for guaranteed separation.

### 4. MEDIUM. Cabinet silhouette is a slab; proportions run tall; coin slots read as eyes [O]
- Profile: `cabinetBody` (line 582) is a full-depth black box under two flat side boards, plus a constant-depth tower to the marquee. Real 1990s uprights step in: a wide lower body, a protruding control panel, a monitor that tilts back, and a sloped or narrower upper cab. In `stop4-model3.jpg` and `stop2-gun.jpg` the side panels read as flat planks.
- Height [I]: defaults `h = 1.95` plus a 0.3 marquee reach 2.25 m; the gun cabinets (`h = 2.15`, lines 798-800, 1038-1039) reach 2.45 m. The screen centre is fixed at `y = 1.38` (line 602), so a dead black band sits between screen top and marquee. It is visible in `stop4-model3.jpg`. Real uprights are roughly 1.8 to 1.9 m overall.
- Coin door [O]: `#2a2a30` plate on a `#121016` body (lines 592-594) is invisible, so two orange rectangles float like eyes (`stop4-model3.jpg`, `stop2-gun.jpg`).
- Remedy: (a) one `ExtrudeGeometry` side profile per (h, d), cached, used for both side panels with bevel about 0.006; (b) derive screen y from `h` (`y = h - 0.55`) and cap `h` near 1.75 to 1.8; (c) lighten the coin door to brushed metal, add a bevelled bezel (`RoundedBoxGeometry`), vertical slots and a small red return button; (d) optional side-art stripe via a shared canvas map (no words).
- Holstered guns on the panel (lines 618-628) are 3-box blobs; reuse `gunModel('slide')` plus a `TubeGeometry` cable on a `CatmullRomCurve3`.

### 5. MEDIUM. Sit-down racers: black seat voids, backrest sign, ghost wheel occluded [O/I]
- `stop3-racing.jpg`: the seat (`#1b1b22`, roughness 0.8, lines 662-664) is the largest object in frame and renders as a featureless black slab. The ghost controls, the stop's subject, are partly hidden behind the backrest.
- Backrest [I]: `rotation.x = -0.15` (line 663) tilts the top toward the dash, so it leans forward. Flip to +0.15.
- Wheel (lines 654-657) is a ring plus hub: fine at distance, thin at spoke level. Ghost wheel nearly overlaps the solid wheel (ghost at `z 0.62`, solid at `z 0.43`) and reads as a doubled ring.
- Remedy: upholster the seat in a cabinet-tinted mid value (about `#3a3550`) with a side bolster using `RoundedBoxGeometry`, a headrest bump, and cut the backrest to about 0.5 m so the wheel stays visible from the aisle. Make the wheel rim 0.03 tube and add three spoke boxes at 120 degrees.

### 6. MEDIUM. Prize counter and claw machines: glass has no edges; the "file" props, the stop's concept, do not read [O]
- `stop7-prize.jpg`: glass cases are frameless 16% opacity boxes (line 825), so the counter edges disappear. Shelf bobbles read well, but the cream "file" boxes (lines 845-847, `BoxGeometry(0.18, 0.24, 0.03)`) are invisible next to them, although "Everything is a file" is the card text.
- Remedy: add `LineSegments(EdgesGeometry(...))` frames (one draw call per case) plus 4 thin corner posts; replace file boxes with an extruded folder shape (tab plus body, 0.012 depth, 0.3 m tall) in a distinct manila or teal, placed at front row.
- Claw: the same frameless glass (line 695) reads as a ball pit; plush are smooth icospheres (line 697, detail 1 gives smooth normals) so they look like gumballs (`stop1-motion.jpg`). Remedy: one merged plush geometry (body sphere, head sphere, two ear spheres) shared by all 26 instances. Claw prongs are 1.2 cm boxes (line 711) and vanish at 3 m; make them 2.5 cm tapering cylinders on a 5 cm hub with a trolley rail.

### 7. LOW. Controllers, token, wheel, hoops
- `controllerModel` (lines 948-966) [I]: the grip is a straight cylinder; ring is a plain torus at 8 radial segments. A `LatheGeometry` handle with a waist and a slightly open ring would sell the Touch-like lookalike. Not visible in any capture.
- Token (lines 781-791): a bare cylinder with no raised rim. Add a `TorusGeometry(0.30, 0.018)` brass rim so the spin catches a highlight.
- Ticket wheel (726-747): no ticket slot or button; hoops (749-761) have a glowing ring and no net (use an open-ended tapered `CylinderGeometry` with an alpha stripe map). Neither appears in any capture.
- Kiosk consoles (1050-1056) [O]: the Wii-like white slab and PS2-like black slab are barely visible in stops 5 and 6. Scale about 1.6x, give each an emissive edge, and put a `controllerModel` beside it.

## Lookalike-not-copy check (docs/guns.md section 2.2): pass

- No text, logos, model numbers or regulatory marks on any gun or controller [O: `gunModel` and `controllerModel` use plain colours]. Shapes are generic boxes and cylinders [O]; the orange pad pistol, white shell with cyan trim and ring controller evoke their real counterparts only by silhouette and palette, as the doc intends.
- Caution for finding 2: draw the extruded profiles from your own proportions, not a trace of a real product outline.
- The console kiosks (white upright with blue slit, black slab with blue edge) are evocative enough; keep them wordless.

## Recommended sequence

1. Fix gun value and bloom (finding 1), then gun construction (2). Both are localised to `gunModel`.
2. Re-seat the gun at stops 5 and 6 (3).
3. Cabinet side-profile cache plus height and coin door (4); it feeds every upright.
4. Seats and backrest (5), then glass frames, file props and plush (6).
5. Low items as time allows.

Dependency: draw-call count. About 20 uprights at roughly 17 meshes each plus per-call `std()` and `glow()` materials (lines 69-74) already multiply materials; adding bevelled parts should go through a shared geometry cache and merged meshes. Hand that to the performance reviewer; not measured here.

## Acceptance checks

- Re-capture stop 2 on all five models: silhouette readable with bloom on, no clipped white.
- Turntable the gun through 360 degrees at stops 2, 5, 6: no gap at the guard, grip rakes rearward.
- Stop 4 and stop 3 re-capture: no dead black band under the marquee; coin slots read as a door; seat has visible form; ghost wheel visible.
- No text or marks on any gun model.

## Remaining uncertainty

Grip-rake and backrest-lean signs are inferred from rotation maths; confirm in a close capture before editing. Controller, ticket wheel, hoops and token were not seen in captures. Height proportions are compared against general real-world dimensions, not measured references.
