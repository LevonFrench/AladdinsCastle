# Desktop controls component

`ac::ControlsViewModel` is a display adapter for a resolved v0.1 controls object.
`qrc:/controls/ControlsView.qml` takes a `controlsModel` object and fills its
parent width; its implicit height is suitable for the detail page's scroller.
It needs Qt Quick/Controls, not Qt Quick 3D, an XR session, game files or a GPU.
The parent Hub remains responsible for embedding, data resolution and profiles.

## Native seam and runtime independence

The model accepts `loadResolved(QVariantMap, previewDirectory)` or
`loadResolvedJson(QByteArray, previewDirectory)`. The JSON may be one resolver
result or the CLI's envelope containing exactly one game. Required fields are
`game_id`, `model` and `data={version,title,element,unmapped_part}`. Elements carry
the resolver's explicit `availability={state,reason}`; absent/unknown states are
unverified. This class never forwards button events to a game or grants support.

A build/package step can export the baseline for all 152 games:

```powershell
python tools/control_sets.py --allow-unmapped --export-view-catalog build/controls-view-catalog.json
```

The native Hub then reads that JSON and calls `loadCatalogGame(bytes, gameId)`.
The player does not need Python. Python is only a source/build/test tool.
The export contains canonical baseline data, never proposed overrides or live
backend declarations. It retains unknown source extensions and explicit gaps.
It is not runtime readiness and must not override the player's configurations.

For production pack/user changes, `ac::ControlsResolver` now applies the
v0.1 rule order and recursive/ID-array/`!delete` layering, then recompute binding
availability from actual backend/profile declarations and pass the resulting
object to `loadResolved`. Unknown extensions must remain in that resolved data.
The host still supplies actual backend/profile values and embeds the result.
Loading the baseline alone does not implement overrides;
do not shell out to Python on the player's machine or silently ignore changes.
The complete original resolved object stays in the model; display rows are a
projection, not replacement configuration data.

## Native resolver API

Call `ControlsResolver::loadLibrary(portableRoot,error)` at catalog load/refresh,
then `resolveGame(gameId,options,result,error)`. Successful results feed
`ControlsViewModel::loadResolved` directly. Failure leaves the caller's result
unchanged and returns an explicit error. Only configuration TOMLs are read.
No production code invokes Python, starts a renderer or sends an input.

`options.packIds` contains the caller registry's explicit priority order.
Each pack's per-game `game.toml` is layered before the per-game user `game.toml`
to select `controls.gun_model`. The selected shared control set is then merged
with the canonical game setup, each pack's shared/per-game setup fragments,
and the user's shared/per-game fragments. Built-in→pack→user order stays intact.
`userOverrideDirectory` defaults to the portable `user/overrides` tree and must
remain a confined relative path; `useUserOverrides=false` is useful for source
previews. No implicit proposed-game-overrides layer exists.

Selected model metadata similarly retains pack/user changes and unknown fields.
The result's `model_metadata` is the exact layered dictionary for the lead's
gun preparation code. `decodedNodesForModel(metadata)` is the preferred callback
for a successful decode/cache of that exact asset; absent results mean nodes
remain unverified. The simple `decodedModelNodes` map is useful for synthetic
tests; the host must not reuse stale node names after an asset/metadata change.
This resolver does not duplicate GLB decoding, material preparation or motion.

`options.backend` accepts the existing explicit input-declaration dictionary.
Unsupported/undeclared actions stay unavailable/unverified and create gaps.
No catalog text creates active bindings. The unresolved parts and all original
coverage labels remain intact. Model files must be GLBs in gun asset directories;
all configuration paths reject absolute/traversal/drive/ancestor-symlink escapes
before reading. Unknown ID-array extensions keep their opaque IDs while known
controls still require valid schema IDs. Date/time extension scalars follow the
existing Hub catalog converter's canonical textual convention.

`profileDefaults` and `profileModel` are **explicit caller options only**.
No profile file format or persisted profile layer is introduced. Optional caller
defaults initialize baseline data so canonical/pack/user-specific keys can win;
the caller remains responsible for supplying already-resolved policy values.
The provisional default model is used only when layered game `gun_model` is
absent. Owner profile UX/persistence remains undecided. The view reads the final
`policy.p1_hand`, clearing any previous pressed state when data is replaced.
Every replacement resets to right when that key or the policy table is absent
(including a resolved `!delete`); it never inherits the preceding game's hand.
Both resolvers use this same assignment for primary and fallback collisions:
slot 0 is the resolved primary hand and slot 1 is the opposite hand.

The current registry supports the 23 registered models. Custom model registration
and profile/pack registry discovery belong to the host's import/settings service;
unknown model IDs fail explicitly instead of silently using another model.

`setPrimaryHand("left"|"right")` changes slot→hand labels (A/X, B/Y) and clears
pressed state for a transient desktop preview. Replacement clears that override.
For runtime hand switching, the host supplies the hand to resolution first and
loads the validated result; changing view labels alone does not validate input
collisions. No hand choice is persisted here.
`setTwoGunsActive(bool)` chooses the one-handed reload fallback
for an explicit dual-gun preview or joined state; backend capacity alone never
means player two has joined. Host tracking/focus/game exit must call
`clearBindingStates()`. `setBindingState(hand,logicalControl,pressed)` highlights
parts/callouts and the controller diagram; it neither emits input nor promotes
an unavailable mapping. Off-screen reload receives the composite logical
`offscreen_trigger` event only after its aim predicate is satisfied.

## Preview data

Pass the local `assets/guns/preview` directory. The model reads only
`<model>.json` and a confined image basename from the selected view. It requires
512×512 images, matching model/version and normalized finite projections.
Traversal and symlink escapes are rejected. Missing/corrupt preview data yields
an explicit placeholder; callouts remain in the list without invented positions.
Callout markers group identical node anchors across hands instead of stacking
unreadable labels. Virtual parts remain in the list because they have no gun node.
Original CC0 Touch controller SVGs are embedded under `/controls/icons/`.

## Parent wiring (Hub lane)

Add `add_subdirectory(src/controls)` after Qt setup, and link `accontrols` into
the Hub UI/services target. Create one model owned by the appropriate detail
controller. Refresh it when the selected game/setup/profile changes, with a
native resolved object and local preview directory. Clear it on leaving detail.

```qml
Loader {
    width: parent.width
    source: "qrc:/controls/ControlsView.qml"
    onLoaded: item.controlsModel = detailControlsModel
}
```

Set the component's panel/text/muted/accent colours and text size from existing
Hub theme tokens. The component's standalone defaults match the current dark
palette; it imports no parent singletons. It has no rebinding/editor actions.

## Standalone tests

Using an existing Qt 6.8.3 SDK and compiler, with no downloads:

```powershell
cmake -S hub/src/controls -B build/controls -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH='<existing-Qt-SDK>' -DAC_CONTROLS_TOML_INCLUDE_DIR='<existing-toml++-include>'
cmake --build build/controls --parallel 2
ctest --test-dir build/controls --output-on-failure
```

The test binary forces offscreen/software rendering. Windows uses its existing
font directory when needed. The repository's Qt receipt wrapper preserves logs.
Tests adapt all 152 resolver results, load eight representative QML scenes,
exercise mirrored labels, one-/two-hand reloads, pressed/unavailable/unmapped
states, native catalog loading, real synthetic image projections and rejection
of invalid/escaping manifests. Software captures cover desktop and narrow layout.
Captures are synthetic/source-only; no actual gun assets or gameplay acceptance.
The native suite compares all 152 baseline results and synthetic actual
pack/user TOML layers with Python, including model precedence, tombstones,
unknown fields, integer types, collisions and path confinement. Python is an
independent test oracle only. Parent Hub builds reuse their existing toml++
target; standalone builds require an existing header directory and never fetch it.
