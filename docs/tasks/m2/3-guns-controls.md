# Session 3: Guns and controls

You own the models, the control data and the view that explains them: `assets/`, `data/guns/`, `data/controls/`, `hub/qml/controls/`, `hub/src/controls/`, `tools/gun_models.py` and the asset checkers. You report to the lead session. Read [README.md](README.md) first: lane table, rules for every session, report format.

## Goal

For every game, the player sees the real control (the gun, later the wheel or the handlebars), sees each part move when they use it, and can read at a glance: **this part does this in the game, and it is this button on your controller.**

## 1. Gun models

- Build tier 1 exactly as [guns-models.md](guns-models.md) says (seven models, parametric Blender scripts, built `.glb` files, CPU previews, a pure-Python checker). Then tier 2, then tier 3 ([guns.md](../../guns.md) §2.1).
- Blender: ask the owner before installing or using it, and say which version. Headless and CPU only; no GPU renderer.
- **Lookalikes, not copies** ([guns.md](../../guns.md) §2.2): original shapes, no logos or brand names, nothing imported, traced or extracted.
- 14 game assignments are marked `review` in [`data/guns/defaults.toml`](../../../data/guns/defaults.toml) because no source describes the gun's shape. When you find a manual or a photograph that settles one, record the source in the model's file and clear the flag. Reference notes: hub wiki topic `vr-arcade-gun-racing`, article `light-gun-peripherals`.

## 2. Moving parts

Every model's moving parts are named nodes with limits ([guns.md](../../guns.md) §2.3): trigger, slide or kick for recoil, pump, selector, buttons, yoke axes on mounted guns. Add a small `[motion]` table to each model's file: node, kind (rotate or slide), axis, range, and what drives it (an input, or an event such as "shot fired"). The runtime and the previews both read it.

## 3. A control set for every game

A control set says, for one game: cabinet part → game input → default controller binding.

- Format: agree it with the lead in the first two days (the lead publishes it). Start from [controls-catalog.md](../../controls-catalog.md) §6.3 and [controls.md](../../controls.md) §1.2.
- Write them **by rule first**, like the gun models: `data/controls/defaults.toml` gives each gun model its default set (trigger → fire → trigger; pump → reload → flick; pedal → cover → grip; selector → B/Y; Start and coin → A/X), with a game's `setup/controls.toml` overriding where a game differs.
- Two guns: each hand gets the same set, mirrored ([guns.md](../../guns.md) §1.2). When both hands hold guns, parts that need the other hand fall back to their one-handed binding.
- Follow the rules the research supports (hub wiki, article `vr-control-setups`): joining on a button, per-hand gun angle, reloads that work one-handed and seated, laser toggle on a thumbstick click, click-to-hold as well as hold for every grab.
- Extend `tools/gun_models.py` (or add a sibling tool) so one command prints every gun game with its model, its control set and anything missing, and fails on gaps.

## 4. The universal controls view

One component, driven only by data, for any game:

- The control's picture with a callout on each part.
- Each callout reads **part → what it does in this game → your controller's button**, for example "Pump → Reload → flick up" or "A / B → Pedal → right grip".
- A second panel shows the player's controller (start with Quest Touch, left and right) with the same bindings highlighted from its side. Draw our own simple controller outlines as SVG; no vendor art.
- Pressing a bound button highlights the part and its callout.
- **Desktop first, and GPU-free:** each model's build exports preview images (front and three-quarter) and a small file with every node's 2D position in each image, so the view is an image plus callouts and needs no 3D runtime. It lives in `hub/qml/controls/` with its model in `hub/src/controls/`; lane 2 places it on the detail page.
- **In VR** the same data labels the real 3D model in Control Mapping mode ([controls-catalog.md](../../controls-catalog.md) §7). The lead's libacvr draws it; you supply the data and the label layout.
- Rebinding from this view comes after it displays correctly.

## 5. After the guns

- Racing: the ghost control models (wheel, the shifters, pedals shown as triggers, handlebars, sticks) and their control sets, by control type from [controls-catalog.md](../../controls-catalog.md) §3.
- Game outputs to haptics: which output of a game (recoil solenoid, lamp, force feedback) maps to which haptic pulse, as data per control set.

## Done when

- Tier 1 and tier 2 models pass the checker in CI, with previews.
- Every one of the 152 gun games resolves to a model and a complete control set, checked by a tool in CI.
- The controls view renders correctly from data alone for every gun game, with tests that load a sample of games offscreen and check that every part has a callout and every callout has a binding.
