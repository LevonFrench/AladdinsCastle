# Pinball route display

`vpx-openxr.toml` is the source for `view.json`. Generate with
`python tools/pinball_controls.py --write`; check without `--write`.
The Python `resolve_route('vpx-openxr')` and C++
`ControlsResolver::resolveRoute('vpx-openxr', out, error)` return the existing
controls-view format. The synthetic ID `pinball-vpx-openxr` is a route selection,
not a catalog or table identity. `quest-controllers` describes the controller
illustration; there is no gun asset requirement.

The layout follows the universal controls contract (2026-10-25). The Hub writes it
into its copy of VPX's settings for every headset launch
(`ac::pinball::headsetControls`, `hub/src/core/pinball/Gamepad.cpp`):

| Intent | Control | VPX action |
|---|---|---|
| U1 Start | Left stick click | `Mapping.Start` |
| U2 Coin | Right stick click | `Mapping.Credit1` |
| U3 Recentre, U3p scale, turn and move the table | Both stick clicks, held | `Mapping.VRControllerCalibration` (VPX's "Align VR view using controllers") |
| U4 Quit and save | Both stick clicks, held 3 s, hands still | Only in the patched VPX (`setups/vpx-vr`): it presses VPX's own Exit Game. Standard VPX: none; leave with the left Menu button and the Hub panel's Exit game |
| Plunger button | A | `Mapping.LaunchBall` |

The menu buttons are bound to nothing, and Y no longer exits. Flippers, grips
(Magna-save), X (in-game menu), the left stick (nudge) and the right stick
(plunger) are VPX's own.

What standard VPX's settings cannot do: time a hold (the 3 s quit; the patched build in `setups/vpx-vr` adds it), wait 150 ms for a
second click (so Start or Coin can fire just before a chord), or tell Start from
a chord; nudge by a physical push (VPX reads only the controllers' buttons and
sticks). The view is read-only: every action has planned execution availability
until a headset session accepts it.
