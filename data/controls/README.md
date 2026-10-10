# Gun control sets: source coverage, pending acceptance

The v0.1 contract is [control-set-format.md](../../docs/control-set-format.md).
`defaults.toml` explicitly maps each of the 23 gun models to one shared set.
Model selection reuses the gun-assignment resolver; the 152 gun games therefore
retain the same model provenance, 94 two-gun eligibility decisions and six
separate-view exceptions. A configured second slot is not backend readiness.

Each shared set explicitly names player/slot 0 and 1; applicable slots are
selected from catalog eligibility and the two-gun option. Fire/reload remain
slot-local. Pump reload includes `flick_up` as its seated, one-handed fallback.
Laser uses the thumbstick click. Coin uses secondary (B/Y). Where the model has
a documented Start button, its candidate binding uses primary (A/X).
There is no universal Start input: Time Crisis's slide-pistol set has none.
These are candidate bindings. Every game input is unverified until an actual
backend declaration advertises it; unsupported inputs remain visibly unavailable.
The resolver never sends input, writes configuration, or accesses game content.

## Check coverage

Run these portable commands from the repository root:

```powershell
python tools/control_sets.py --allow-unmapped --table
python tools/control_sets.py --allow-unmapped --game timecris --json
python tools/test_control_sets.py
```

`--allow-unmapped` is explicitly SOURCE ONLY. Default `python tools/control_sets.py`
fails on missing mappings, undeclared backend/runtime inputs or absent models.
Coverage is not completeness. `--json` includes each model, shared set, layers,
resolved elements, unresolved parts, availability and categorized gaps.

Actual model-node references are checked only when the GLB exists. Absent assets
are labelled `not-built` and create an asset gap. This tool's conditional check
only verifies references; `tools/check_gun_assets.py` remains the full asset gate.
No haptic output channels are supplied without verified backend event evidence.

## Unresolved parts and proposed per-game overrides

`[[unmapped_part]]` is a lead-approved extension, not a game input. It carries
`id`, `slot`, `player`, `node`, `label`, `reason`, and `requested_semantic`.
The requested semantic is diagnostic text, not a made-up ABI port. Model buttons
must have a control element or an explicit unresolved row. Scope/zoom, nozzle
twist and second-hand grabs also remain unresolved where v0.1 lacks a binding.
Desired grab hold/toggle, mirrored angle and join policies remain data intent;
they do not claim implemented runtime actions.

`proposed-game-overrides/` contains inactive cover proposals for lead review.
The lead places approved fragments in `games/<id>/setup/controls.toml`.
These proposals never become an implicit layer; inspect them explicitly with:

```powershell
python tools/control_sets.py --allow-unmapped --proposed-overrides --game timecris --json
```

Time Crisis's proposal binds a grip hold to pedal-down/expose and release to
cover. There is no physical ducking and no Start port. Other cover proposals
come from the existing pedal metadata and cover design, and still need a matching
backend declaration. Time Crisis 5's second same-player pedal remains an explicit
gap. Virtua Cop's catalog pedal entry is flagged as uncertain and has no proposal.
No racing files or canonical per-game overrides are changed by this lane.

## Source API for Hub integration

`tools/control_sets.py` exposes `Resolver(root)` and
`resolve_game(game_id, backend=None, pack_overrides=(), user_override=None)`.
The optional `proposed_override` argument is for explicit candidate inspection.
The returned dictionary/CLI JSON schema includes:

| Key | Meaning |
|---|---|
| `model`, `model_provenance`, `shape_review` | Gun choice and confidence |
| `control_set`, `layers`, `data` | Shared set and merged source data |
| `two_gun_eligible`, `separate_views`, `configured_slots` | Catalog/options |
| `declared_active_slots` | Intersected actual backend counts/shared-view support |
| `node_validation` | Built-reference check or explicitly not built |
| `gaps` | Missing part, binding declaration, asset or game requirement |

Every `data.element` has part → `label` (game action) → logical `binding` and
`availability={state,reason}`. A supplied backend declaration has `guns`,
`players`, explicit `shared_view`, `control=[{kind,semantic,player}, ...]`,
and optional `runtime_actions`. Omitted declarations never imply support.
The UI should show unavailable/unverified rows; only available actions are
eligible for later runtime emission. Actual runtime OpenXR profile resolution
and controller labels stay in the Hub/runtime integration, not in this script.

Layering is shared set → canonical game override → supplied ordered pack layers
→ user override. Tables merge recursively, ID arrays merge by ID, other arrays
replace, and `!delete` removes keys. Unknown extension keys survive. Versionless
gun overrides are rejected; existing versionless racing files are untouched.
