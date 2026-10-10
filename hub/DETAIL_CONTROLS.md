# Desktop controls review

The desktop detail page embeds the existing native controls view. The original
emulator/setup controls table stays available, including for racing entries that
have no gun control set. Configuration errors clear the previous game's view and
appear beside the controls section. Missing previews use the component's explicit
placeholder; no gun model is generated, downloaded or decoded by this integration.

`DetailControlsController` owns one `ControlsViewModel` and retains the entire
resolved dictionary separately from display rows. This preserves layered
`model_metadata`, unknown extensions, node diagnostics and coverage gaps.
`CatalogData.root` and `packIds` carry the actual catalog root and its stable
priority/name order to native resolution; the Hub does not sort a second registry.

Library loading and pack/user TOML resolution run in a worker. Generation checks
prevent old game/context results from replacing the current selection, including
after leaving detail. Only a matching configuration epoch may populate the
immutable library cache. Game/setup selection and saved-setting notifications
refresh the view; Reload controls re-reads configuration. A refreshed catalog
must call `configure(root,packIds)` with its new registry snapshot. No production
Python process is used.

The Hub currently supplies no live backend declaration and no decoded-node
evidence. Public runtime creation is unsupported. Availability and model nodes
therefore remain unverified; source coverage is not install/runtime acceptance.
Existing install-only `left_handed` settings are not a runtime profile. This
change introduces no profile format or persistence layer.

`setOptionsSource` is a typed C++ seam for an explicit host context: it receives
the selected game/setup on the UI thread. Future backend/profile values must be
actual declarations. `decodedNodesForModel` receives exact layered metadata in
the worker and must use an immutable or thread-safe successful-decoder snapshot;
do not provide private runtime objects or stale names keyed only by model ID.
Synthetic evidence is supplied only in tests.

Leaving detail clears rows and pressed state. Desktop focus loss and game exit
clear highlights. A future tracking/input host must call
`UiController::clearDetailControlStates()` on tracking/focus loss and use only
explicit joined-player evidence for dual-gun state. The display sends no input
to a game and grants no runtime support. Preview assets and actual headset
interaction remain separate acceptance gates.
