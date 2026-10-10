# M2 prep: build the tier 1 gun models

Design: [guns.md](../../guns.md) (read it first, §2 in full). Model list and metadata: [`data/guns/`](../../../data/guns/). This task builds the seven tier 1 models. It needs **no GPU**: Blender runs headless from scripts.

## Ground rules

- Same rules as M1 ([tasks/m1/README.md](../m1/README.md)): privacy, no system changes without the owner (ask before installing Blender; say which version), tests in CI, a report in the PR.
- **Original lookalikes only** ([guns.md](../../guns.md) §2.2). No logos, brand names, model numbers or regulatory marks on any model. Never import, trace or convert someone else's mesh, a scan or anything from a game. You may look at photographs to judge proportions; build every shape from primitives and your own parameters.
- Do not edit `docs/libacvr-contract.md`, `docs/game-schema.md` or `tools/validate_catalog.py` in this task (another pull request owns them).
- One branch (`codex/gun-models`), one PR to `main`.

## Build these (tier 1)

| Id | In one line |
|---|---|
| `generic-pistol` | Plain compact pistol: the fallback |
| `arc-pistol-slide` | Namco-cabinet-style pistol with a slide that travels back |
| `arc-pistol-twin` | Sega-cabinet-style stocky pistol with a moulded cylinder |
| `con-pistol-slim` | GunCon-style slim pistol, A and B on each side under the barrel |
| `con-pistol-dpad` | GunCon 2-style: d-pad at the rear, A/B above the trigger, C under the grip, Select and Start on the left |
| `con-revolver-chunky` | Virtua Gun / Stunner-style chunky revolver with a Start button |
| `mnt-mg-heavy` | Heavy machine gun on a yoke with rear handles |

Each model's `data/guns/<id>.toml` gives its `shape`, `length_mm`, `features`, `[tints]` and `[[button]]` list. Treat `length_mm` as the target overall length.

## Deliverables

1. **`assets/guns/src/common.py`**: shared helpers (primitives, bevels, node naming, material slots, glTF export, level-of-detail generation).
2. **`assets/guns/src/<id>.py`** per model: a parametric Blender Python script, runnable as `blender --background --python assets/guns/src/<id>.py -- --out assets/guns/<id>.glb`. Parameters at the top (lengths, grip angle, barrel size), no hand-edited meshes.
3. **`assets/guns/<id>.glb`** per model: the built output, committed. Plus `assets/guns/build.py` (or a script target) that rebuilds all of them.
4. **Format** ([guns.md](../../guns.md) §2.3):
   - glTF 2.0 binary, metres, +Y up, −Z forward along the bore.
   - Nodes: `grip` at the hand position (origin where the web of the hand sits, −Z along the bore, so libacvr can lay it on the OpenXR grip pose), `muzzle`, `sight_front`, `sight_rear`, `pivot_trigger`, `fx_muzzle`, `fx_laser`, and one `button_<id>` per `[[button]]` in the model's toml. Add `slide_recoil` + `limit_recoil` where `features` has `recoil_slide`, and `pivot_yaw` + `pivot_pitch` + `grip_two` for `mnt-mg-heavy`.
   - Materials named `body`, `accent`, `dark`, `glass`. `body` and `accent` use neutral base colours (the runtime tints them from `[tints]`); export one variant only.
   - Two levels of detail in the same file: `LOD0` up to 8,000 triangles, `LOD1` up to 2,000. No textures in tier 1 (flat colours and vertex colours only).
5. **Previews:** `assets/guns/preview/<id>.png`, 512 × 512, rendered headless with **Cycles on the CPU** at low samples, one three-quarter view per model in its `p1` tint, plus `assets/guns/preview/sheet.png` with all seven. No GPU renderer.
6. **`tools/check_gun_assets.py`** (pure Python, no Blender, no third-party packages): parses each `.glb` and checks it against its toml: required nodes present, one `button_` node per button, material names, triangle budgets per LOD, bounding-box length within 10% of `length_mm`, −Z bore axis (muzzle in front of grip), no embedded images, no extra root nodes. Run it in CI next to `python tools/gun_models.py`.
7. **`assets/guns/LICENSE`**: CC0-1.0 for the built models, with one line saying the scripts are GPL-3.0 like the repository (D47).
8. **`assets/guns/README.md`**: how to rebuild, the node table, how to add a model, and the lookalike rule.

## Done when

- The seven `.glb` files, scripts, previews and the checker are in the PR, and `tools/check_gun_assets.py` and `tools/gun_models.py` pass in CI on Windows and Linux.
- Rebuilding from scripts reproduces the committed `.glb` files (same node names, budgets and bounds; byte equality is not required).
- The PR shows the preview sheet and lists, per model, the triangle counts and anything you could not match from the shape notes.
- Nothing in the PR is copied from an existing model, and no trademark appears in a file name, node name or texture.

## Not in this task

Tier 2 and 3 models, the libacvr loader and renderer, hands, sounds, and the Hub's gun picker.
