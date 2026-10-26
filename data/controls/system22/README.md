# System22 route-native controls

Single authored source: [controls.toml](controls.toml). It uses the explicit `system22-native/1` namespace inside the existing0.1 view envelope. It does not change the arcade ABI or its generic control-set schema. The dedicated source adapter reuses the catalog/model defaults, TOML loading, ID-array merging and actual GLB node decoder. Compiled view data is derived, never hand-edited.

D63 order is Time Crisis, Rave Racer, Ace Driver, Dirt Dash, Tokyo Wars, Prop Cycle, Armadillo Racing. TC2 stays deferred until Rave Racer is steerable; its separate-view issue is unresolved.

| Game | Control set | Model |
|---|---|---|
| timecris | One active arcade pistol | Existing arc-pistol-slide |
| raverace | Wheel, gas/brake, shift up/down, View | No model built; no gun substitute |
| acedrive | Same host action interface, own ADC bounds | No model built; no gun substitute |
| dirtdash | Wheel/pedals/shifts/View plus motion-stop question | No model built; no gun substitute |
| tokyowar | Wheel, forward/backward pedals and two handle triggers | No model built; no gun substitute |
| propcycl | Two-axis handlebars and cycling pedals | No model built; no gun substitute |
| adillor | Trackball, Start and coin | No model built; no gun substitute |

Shift-up/down is the pinned host's interface, not proof of physical shifter geometry. The catalog's Rave Racer `hl` description remains a question for the owner. Tokyo Wars' trigger names come from source; their precise game captions remain questions. Trackball deltas and cadence remain their own signals, not invented gun or steering-only inputs.

TC1 has ONE player/gun slot. Fresh left/right trigger edges select that gun's active hand; they are not simultaneous P1/P2 guns. Either grip drives the same cover pedal in grip mode. Options stick controls keep the pinned physical hands.

**Universal layer (controls contract v1).** Every System22 game uses the same stick-click controls: the left click is Start (U1), the right click is Coin (U2), both clicks together recentre the view (U3) and holding both for 3 seconds quits and saves (U4). libacvr resolves them once, for every host, before any binding sees them: a click waits 150 ms for the other, so a chord never sends Start or Coin. Start and Coin here are `press` bindings (the host still gets the level held for as long as the click is held). Recentre and quit are runtime actions of the chord and have no per-game row. The left Menu and the right system button are never bound; under ALVR the left Menu opens the SteamVR dashboard.

**Time Crisis face buttons.** Saved-primary B/Y is the laser (M4); the saved-primary hand follows the saved layout, not the trigger handoff. Pause and options: the old left-stick-click pause could not stay, because that click is Start now. Pause moved to the **saved-off hand's A/X**, the button the old recentre used, so nothing else in play changed hands. A Start-press pause was rejected: arcade Time Crisis uses Start for continue, and libacvr cannot know when a game ignores it. While paused, saved-off A/X resumes, saved-off B/Y changes cover mode, saved-primary B/Y changes the saved primary hand, and saved-primary A/X resets the gun angle. Those paused rows reuse gameplay buttons, which is safe because gameplay mapping is suspended while paused. The old gun-angle reset on a `menu_chord` is gone, and no row uses `menu` or `menu_chord`. The pinned host's Start input is not confirmed (only the coin bit is), so `start` stays a question until the backend declares it. No offscreen-reload input is invented. Grip mode is the requested cover option; this source adapter does not write settings or disable the host's optional physical-crouch mode.

All game/controller availability remains unavailable in this source packet. The adopted host still needs our gun conversion, an actual grip-pose action and the hand-label draw consumer. Wheel/handlebar/trackball controller defaults are explicitly proposed questions; their VR bridge and models are unimplemented. No resolved data sends input, writes profiles, accesses game files or starts an application.

## Source evidence

- TC1 controller actions/paths, saved-layout behavior, trigger selection and MCU bits: [quest_host.c at e5a5f7d](https://github.com/DR-89/time-crisis-vr/blob/e5a5f7d204e4be8e96ed2396ebea3886c833f021/quest/quest_host.c). Lines225-261,373-441,740-755. TC1's engine gitlink is6aaa90b4cbc7a23733e1c9f5f9a5e772a19fe23d.
- Racer cabinet tables use the separately pinned engine a9dab183eb29e941629b473e4172b16835576d94, not an assumed replacement for TC1's gitlink: [Rave input](https://github.com/spacestate1/namco22-decompile/blob/a9dab183eb29e941629b473e4172b16835576d94/raverace/src/rr_input.c), [Ace table](https://github.com/spacestate1/namco22-decompile/blob/a9dab183eb29e941629b473e4172b16835576d94/acedriver/src/ad_game.c), [Dirt table](https://github.com/spacestate1/namco22-decompile/blob/a9dab183eb29e941629b473e4172b16835576d94/dirtdash/src/dd_game.c), [Tokyo table](https://github.com/spacestate1/namco22-decompile/blob/a9dab183eb29e941629b473e4172b16835576d94/tokyowar/src/tw_game.c), [cycle input](https://github.com/spacestate1/namco22-decompile/blob/a9dab183eb29e941629b473e4172b16835576d94/src/input.c), [trackball table](https://github.com/spacestate1/namco22-decompile/blob/a9dab183eb29e941629b473e4172b16835576d94/adillor/src/am_game.c).

Exact source symbols/line ranges and questions are kept per assignment. No third-party model/code or game content was imported.

## Build-time and source API

Run from your checkout with `RepositoryRoot` set to its root.

```powershell
python -B "${RepositoryRoot}/tools/system22_controls.py" --table
python -B "${RepositoryRoot}/tools/system22_controls.py" --export-view-catalog "${RepositoryRoot}/data/controls/system22/view-catalog.json" --table
python -B "${RepositoryRoot}/tools/test_system22_controls.py"
python -B "${RepositoryRoot}/tools/system22_controls.py" --game timecris --primary-hand left --active-hand right --lod 1 --hand-labels
```

Existing Python `Resolver.resolve_game(game_id, route="namco22-vr")` explicitly selects these source rows. Omitting the route retains the existing generic baseline. Unsupported backend/pack/user/model layers are rejected, not silently ignored. Only the saved primary-hand profile option is currently supported.

The native `ControlsResolver` consumes the derived catalog when `ControlsResolveOptions.route` is `namco22-vr`; players need no Python. Hub must supply that explicit route and refresh on selection/hand changes. `ControlsViewModel` can show pending cabinet rows without an invented gun model, and saved face-button roles follow its handedness toggle. Existing offscreen view/native-parity tests cover that behavior. Live route capability still needs the actual backend integration.

`hand_labels(result, active_hand, lod)` is the adopted-host source handoff. It emits real gun-node anchors on the active physical hand, controller anchors for system/pedal actions, and the correct button/path from the saved layout. LOD1 fire uses `pivot_trigger_lod1`; muzzle/laser remain static shared anchors. The caller supplies actual tracked transforms and calibration, transforms those authored nodes at its predicted frame time, and draws labels through its existing renderer. This module does not invent a grip offset, draw a panel, patch the parked host or claim labels already rendered. Model, shot and laser need the SAME calibrated transform, with actual grip position and aim orientation as recorded in the existing TC1 handoff.
