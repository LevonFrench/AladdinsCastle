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

For production pack/user changes, the Hub's **native resolver** must apply the
v0.1 rule order and recursive/ID-array/`!delete` layering, then recompute binding
availability from actual backend/profile declarations and pass the resulting
object to `loadResolved`. Unknown extensions must remain in that resolved data.
That native resolver/profile bridge is a required integration handoff, not part
of this display adapter. Loading the baseline alone does not implement overrides;
do not shell out to Python on the player's machine or silently ignore changes.
The complete original resolved object stays in the model; display rows are a
projection, not replacement configuration data.

`setPrimaryHand("left"|"right")` changes slot→hand labels (A/X, B/Y) and clears
pressed state. `setTwoGunsActive(bool)` chooses the one-handed reload fallback
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
cmake -S hub/src/controls -B build/controls -DBUILD_TESTING=ON -DCMAKE_PREFIX_PATH='<existing-Qt-SDK>'
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
