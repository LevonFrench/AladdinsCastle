# Pinball route display

`vpx-openxr.toml` is the source for `view.json`. Generate with
`python tools/pinball_controls.py --write`; check without `--write`.
The Python `resolve_route('vpx-openxr')` and C++
`ControlsResolver::resolveRoute('vpx-openxr', out, error)` return the existing
controls-view format. The synthetic ID `pinball-vpx-openxr` is a route selection,
not a catalog or table identity. `quest-controllers` describes the controller
illustration; there is no gun asset requirement.

The view is intentionally read-only: source-backed bindings have planned
execution availability. Y exits the table and carries a warning. Recenter is
unavailable because the pinned tag comments out its plugin action map; the
native resolver rejects any catalog that claims otherwise. The native XR
plunger and nudge mappings exist, but the plugin clears the analog state mask.
No host bridge or controller event emitter is included here. The Hub owns the
execution, source capability review and table launch/return acceptance gates.
