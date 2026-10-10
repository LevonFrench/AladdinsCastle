# Control sets v0.1

Published 2026-10-10 by the lead. This is the lane contract, not an implemented
runtime or a claim of game support. The C ABI is in `libacvr/include/acvr.h`.

## Resolution and layering

Lane 3 owns `data/controls/defaults.toml` and `data/controls/<id>.toml`.
The defaults file has `version = "0.1"`, `fallback = "<set-id>"` and
`[[rule]]` entries with a unique `id`, `gun_model` and `control_set`.
First matching rule wins after resolving the gun model using `data/guns/defaults.toml`.
The selected shared set is merged with `games/<id>/setup/controls.toml`.
Pack and user overrides then follow `docs/config-spec.md` layering: recursive
tables, scalar replacement, `element` arrays merged by `id`, and `!delete`.
Unknown keys survive every merge. Versionless existing racing files remain
legacy format; do not silently interpret them as v0.1 or mass-rewrite them.

## Cabinet part to game input to binding

Each v0.1 set requires text `id`, `title`, `version = "0.1"`, and `[[element]]`.
IDs use lowercase kebab-case. Every element has a unique `id`, human `label`,
`part` (cabinet part name), `node` (model node, or empty for a virtual pedal/menu),
nonnegative integer `slot` and `player` (zero-based), and the tables below:

```toml
version = "0.1"
id = "single-gun-cover"
title = "Single gun with cover pedal"

[[element]]
id = "p1-trigger"
label = "Fire"
part = "trigger"
node = "pivot_trigger"
slot = 0
player = 0
input = { kind = "gun", semantic = "trigger" }
binding = { hand = "slot", control = "trigger", mode = "hold" }

[[element]]
id = "p1-cover"
label = "Pedal down / expose"
part = "cover-pedal"
node = ""
slot = 0
player = 0
input = { kind = "axis", semantic = "cover_pedal" }
binding = { hand = "slot", control = "grip", mode = "hold" }
```

`input.kind`: `gun`, `axis`, `button`, or `runtime`. Gun semantics are `trigger`
and `reload`; axis/button semantics are lowercase suffixes of the matching
`ACVR_AXIS_*` / `ACVR_BUTTON_*` constants. Runtime semantics are `laser_toggle`,
`recenter`, `pause`, `hand_switch` and `join`. Runtime actions never become invented
game input ports. A backend's control declarations are authoritative: unsupported
elements stay visible as unavailable in the controls view and send no input.

`binding.hand`: `slot`, `left`, `right`, `either`. Slot resolves through current
hand assignment; two active guns always use `slot` for fire/reload/pedal.
`binding.control`: `trigger`, `grip`, `primary`, `secondary`, `thumbstick_click`,
`offscreen_trigger`, `flick_up`, `pump`, `slide`, or `menu_chord`.
`binding.mode`: `hold`, `press`, `toggle`. A trigger/grip threshold is optional
`threshold` (0..1, default 0.55); `invert` is optional boolean, default false.
Off-screen trigger reload is a composite trigger edge plus off-screen aim, not
a second independent fire event. `primary` means A on right/X on left;
`secondary` means B/Y. Primary press defaults to Start/join, secondary press to
coin unless a game's actual controls override this. Do not bind coin and Start
to the same press. Use the controller profile's actions, never the system button.
The OpenXR profile resolver is runtime-owned; these are logical names, not paths.

For two-hand interactions, optional `fallback_binding` has the same shape and
is required for `pump` or `slide`: it replaces that binding while two guns are
active. Defaults use `offscreen_trigger` or `flick_up`. Grip holds expose/pedal
down (1), release covers (0); optional invert/toggle are user choices. No
head-height ducking. With one gun, `either` grip may drive its one cover pedal;
with two, each slot's grip drives its own player's pedal.

Control sets explicitly expand a second player's elements with distinct IDs;
do not infer player identity from array order. Two-gun eligibility is intersected
with actual backend gun/player counts and shared-view support, never inferred
from a model. Time Crisis on System 22 has one player and one active gun.

## Models, animation and outputs

Model metadata uses `[motion.<id>]` entries: `node`, `kind` (`rotate` or `slide`),
unit-length local `axis = [x,y,z]`, increasing `range = [min,max]` (radians or
metres), and `drive` (`trigger`, `recoil`, `pump`, `selector`, `yaw`, `pitch`, or
`button:<id>`). Input/axis drive values are normalized 0..1 and interpolate the
range. Recoil uses a pulse envelope; optional `duration_ms` defaults to 45.
Optional `lod_nodes = ["canonical", "canonical_lod1"]` selects the corresponding
node in each LOD; every listed node must exist and only the selected LOD draws.
The single `grip` root and static `muzzle`/sights/fx anchors are independent of
the recoil hierarchy. Animation never changes the ballistic muzzle transform.

Optional `[[output]]` entries have unique `id`, zero-based `player`, `kind`
(`solenoid`, `lamp`, `ffb`), integer `channel`, `slot`, `motion` (motion id),
`amplitude` (0..1) and `duration_ms`. Lane 3 supplies data; the runtime matches
actual backend events and emits a pulse on a rising edge, once per native tick.
Unverified outputs remain absent. Overflow, pause or lost tracking cancels
haptics. A fallback fire animation and real recoil output must never double-fire.

## Validation and ownership

Lane 3's checker validates resolved sets, node references, duplicate IDs,
binding collisions, range/axis values, and coverage of every gun game. It must
retain unknown extensions while rejecting malformed known fields. Synthetic
example: `libacvr/tests/fixtures/control-set-v0.1.toml`.
The lead owns schema decisions, C ABI and both catalog validators. Hub owns
embedding lane 3's controls view and distinguishes eligibility from readiness.
Changes to this contract require a lead decision and version bump.
