# Model presentation review: materials, look-dev, lighting

Reviewer lens: Blender texturing/shading (primary) and lighting/rendering (secondary), applied to three.js 0.160. Evidence labels: **observed** (capture or GLB JSON parsed), **inferred** (from source), **hypothesis**. Nothing was run in a browser.

All paths are relative to the repository root.

## Where Blender technique does not translate

- Light linking and light groups do not exist in three.js (lights hit every material). Per-exhibit lighting needs a material-level formula, not a linked lamp.
- "Standard" view transform maps to `NoToneMapping`. Khronos PBR Neutral (the Blender skill's pick for brand colour) is `NeutralToneMapping`, which needs three r162+. The site pins 0.160 (`website/index.html:16`), so it is not available without a version bump.
- Bake-then-threshold advice is moot here: the GLBs carry only POSITION and NORMAL (observed, all 19 files). There are no vertex colours or textures, so the data tints are the entire albedo.

## Ranked findings

### 1. High: solid control models lose their materials (inferred from observed GLB data)
`website/models.js:40-41` looks the material up by exact name (`solid[name] || solid.body`). The control GLBs use suffixed names: `body.006`, `accent.006`, `glass.006` in bars-motorcycle; `.004` in shifter-sequential; `.002` in pedals-dual; `.001` in stick-arcade; wheel-three-spoke and shifter-four-gate are unsuffixed. Every suffixed material falls through to `solid.body`, which for controls (no tint table) is flat `#8c8f98` (`models.js:27`).
- Effect: the bench's six control chips (wheel, shifters, pedals, bars, stick) show as single-colour grey blobs, with no part separation. The GLBs' own palette (navy body, metallic accent at metalness 0.8, teal glass) is discarded. Four-gate shifter (plain names) and sequential shifter (suffixed names) will also differ from each other.
- Remedy: normalise the key (`name.replace(/\.\d+$/, '')`). When `entry.tint` is absent, build the solid set from the source materials (`src.color`, `metalness`, `roughness`, plus `envMap`, with `envMapIntensity` around 1.0 for the metallic accent). Keep the tinted palette for guns only.

### 2. High: glove colour is invented and about half the project's value (observed)
`models.js:25` hard-codes `#3b4150` / `#6b7282`. The project's own glove tint is `glove = { body = "#8293a8", accent = "#a9b8c9" }` in `assets/hands/grip-hand-right.toml`. `website/tools/sync_models.py:34,56` never reads hand tints, so `models.json` has `tint: null` for both hands.
- Effect: the web glove is a dark slate that melts into the black cabinet and black guns (stop-1 capture: the hand is barely separated from the cabinet; `scene-stop1-before-panels.jpg`). The headset glove is a light steel blue.
- Remedy: have `sync_models.py` read `[tints].default` from `grip-hand-*.toml` into `models.json`, and delete the fallback at `models.js:25`. Also apply the same hand to the left side, if ever shown, via the same entry.

### 3. High: the bench does not look like the headset (observed colour sampling)
`bench.js:25,29-31` uses ACES with hemisphere 1.1, key 2.2, rim 1.2 and a RoomEnvironment (envMapIntensity 0.3-1.0 in `models.js:27-30`). The headset formula (`assets/guns/runtime-lighting.json`) is ambient 0.35 plus one camera-space directional 0.65, no specular, no tone mapping.
- Measured from the captures: the red pistol body in `d2-bench.jpg` samples about (163-189, 35-43, 37-44), against the tint `#d2362e` = (210, 54, 46) and the neutral test bench (`models-testbench.jpg`) at (232, 66, 54). The bench red is 15-20% darker and shifted crimson; ACES's filmic shoulder skews saturated reds. The page copy says these are "the same file the Hub's setups draw in the headset", so the colour should match.
- Remedy (exact parity, bench only, since it has its own renderer): `renderer.toneMapping = THREE.NoToneMapping`; `MeshLambertMaterial` for the `solid` style (no specular, no env); `AmbientLight(0xffffff, 0.35 * Math.PI)` plus `DirectionalLight(0xffffff, 0.65 * Math.PI)` parented to the camera at (0.36, 0.70, 0.62) so the light stays in view space while the model turns. (`* Math.PI` because three r155+ lights are no longer pre-scaled.) Keep a weak cyan rim only if value separation needs it (finding 6). This is also the Blender "judge honestly in Standard view, no spec" rule.

### 4. Medium: ghost style stacks and blows out (observed + computed)
`models.js:32-35,41`: one shared additive `MeshBasicMaterial`, colour `#7ff8ff` x1.6 = linear (0.34, 1.50, 1.60), opacity 0.32, front faces only, no depth pre-pass. One layer adds (0.11, 0.48, 0.51); three overlapping layers reach 1.5 in G/B, over the bloom threshold 1.1 (`arcade.js:1112`).
- Effect: brightness follows part overlap, not form. The wheel hub is white-hot in `scene-stop3-ghost-before-panels.jpg`, the pedals read as flat plates (`models-testbench.jpg`), and cyan added over the pale-yellow cabinet washes toward white.
- Remedy: add a colour-less depth pre-pass per ghost mesh (a sibling mesh sharing the geometry, `colorWrite: false`, `depthWrite: true`, `renderOrder: -1`) so only the nearest surface contributes; then a Fresnel alpha (`0.08 + 0.55 * pow(1 - abs(dot(n, v)), 2)` via `onBeforeCompile` or a small `ShaderMaterial`) so the rim carries the silhouette. Keep the body below 1.1 and let only the rim cross it. Use normal blending with a dark cyan base on pale cabinets.

### 5. Medium: exhibits orbit their origin, not their centre (observed GLB bounds)
`arcade.js:933-941` (`realGun`) puts the model root straight into the pivot, which the turntable rotates (`arcade.js:960-964`). The origin is the grip. Parsed bbox centres sit 5-8 cm off axis for pistols (x2.4-3.0 scale = 12-24 cm) and `mnt-mg-heavy` is at (-0.08, -0.14, -0.31) m, so at x1.6 the gun swings on a ~0.55 m radius. `bench.js:59-61` already centres by `Box3`; the arcade does not.
- Remedy: for `hands: false` guns, `m.root.position.copy(center).multiplyScalar(-scale)` after measuring a `Box3` at scale 1. Leave the stop-1 hand-held guns pivoting at the grip (that is correct for a hand).

### 6. Medium: black guns have no value separation; `DARK` inverts the hierarchy (observed)
`models.js:20` sets `DARK = '#2c2a33'` (linear 0.025), lighter than the black bodies `#1b1b1e` (linear 0.011) of funcon, the console pistols and the revolver (`models.json`). Rubber panels read lighter than the shell. `assets/guns/README.md:67` records the project already lifted its dark accent to `#50535a` because the old darks "read as holes". The black body itself sits on the dark bench gradient (`style.css:242`) and the scrim.
- Remedy: derive `dark` from the body (`body.clone().multiplyScalar(0.5)` or `lerp` to black) so it is never lighter than it; raise the bench rim (`bench.js:31`) from 1.2 to about 2.4 for bodies below ~0.05 linear, or lighten the `.bench-view` backdrop slightly. Do not change the tint data.

### 7. Medium: the cycling gun renders eight guns at all times (observed counts, inferred cost)
`arcade.js:995-999` hides inactive guns with `scale = 1e-4`, not `visible = false`, so ~233 LOD0 meshes (35+31+26+33+43+19+37+9, counted from the GLBs) still go through the draw loop at the gun stop while one is shown. Remedy: `g.root.visible = (k === n)` alongside the scale animation.

### 8. Low: the models are mostly behind panels (observed)
`d1`, `d4`, `d5` and `p1` show no gun or glove at all; only a sliver of ghost wheel is visible in `d3`. The scrim also dims what is visible by 18-60% (`arcade.js:1353`, `style.css:161-166`). The lazily loaded guns on four stops and the lighting parity work above therefore mainly pay off in the bench. If the arcade is to stay a backdrop, nothing breaks; if the models should read, shift the subject into the open column (`camera.setViewOffset`) rather than brightening them.

### 9. Low: bench has no scale cue (observed)
`bench.js:63-65` frames every model to fill the view, so a 154 mm remote and a 650 mm mounted gun look the same size while the copy says real size. Remedy: add a 100 mm scale bar, or show the length chip more prominently (it is in the meta line); the grip hand GLB could serve as a ghost size reference.

### 10. Low: muzzle flash is invisible (inferred)
`arcade.js:938`: a 1.2 cm sphere (x2.4-3.0), colour `#ffd27a` at opacity 0.8, peak about (0.8, 0.5, 0.16) linear, so it never reaches the 1.1 bloom threshold. `toneMapped:false` (`arcade.js:909`) is ignored under the composer. Remedy: `flash.material.color.multiplyScalar(2.5)` and a larger radius (about 0.03).

## Checked and fine

- LOD1 hiding works for all 19 GLBs: every LOD1 mesh sits under a `LOD1` node, none leak (observed).
- Hand attach: every gun in `assembly.json` binds node `grip`, which exists in all models, with the 0.1305 quaternion; `loadModel` reads translation and rotation correctly.
- Trigger/recoil drive and the `Fire` button follow the data tables.
- Tint data in `models.json` matches the `[tints].default` in the TOMLs for guns.
