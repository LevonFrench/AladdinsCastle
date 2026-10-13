# Original cabinet controls

Parametric sources live in `src/`; built GLBs beside this file. Model motion/label metadata is in `data/controls/models/`. The assets use metres,+Y up,−Z forward, four material slots and `LOD0`/`LOD1` groups. The cabinet root is `control_root`; moving parts and anchors belong to their LOD group with `_lod1` aliases. Rubber is geometric grip/tread detail with a rough dark material, metal is bevelled/smooth and metallic, and plastic uses a distinct coloured shell. No external texture, mesh, font or game content is loaded.

Run installed Blender in background/factory mode with two CPU threads:

```text
blender --background --factory-startup --threads 2 --python-exit-code 1
  --python assets/controls/build.py -- --models wheel-three-spoke shifter-four-gate pedals-dual panel-view-four
  --renders <private-output-directory>
```

Each model also has its own script with `--out` and optional `--render`. Scripts are GPL-3.0-only, built assets and original vector pictures CC0-1.0. Full licensing is in LICENSE. Gun previews import only this repository's already-original gun GLBs; they do not rebuild or alter them.

`tools/model3_control_art.py --step 1` (or2/3/4) authors the corresponding legacy cabinet sets, per-game files in `data/controls/cabinets/games/`, holds and vector cards. The data uses the existing versionless element/output/extractor format; no legacy sets are migrated and no new schema/ABI vocabulary is created. Consumers use the exact Supermodel names in output tables and preserve extension fields. See `docs/model3-controls/emulator-handoff.md` for motion, input and instance rules.
