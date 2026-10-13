# Model 3 controls: emulator handoff

Read `data/controls/cabinets/games/<id>.toml`: complete per-game files in the existing versionless cabinet format (config-spec §5). The nongun titles also publish conventional `games/<id>/setup/controls.toml` copies. Gun games keep their generic v0.1 mappings untouched; the Model 3 reader uses the native cabinet file. `[hold].default` selects one `[holds.<id>]` card. Selecting another merges its `data/controls/cabinets/<control_set>.toml` over the baseline by element ID; keep unknown fields. Light/dark pictures and plain descriptions live in the same chosen hold. No new control-set version or ABI ports are introduced.

Each element supplies its logical VR `source`, `extractor`, calibration/range, direction, grab rule and exact `output.axis`/`output.button` name. `primary` means A/right or X/left; `secondary` B/right or Y/left. Resolve these through the controller profile. Feed the exact named CInputs object, never a guessed keyboard binding. Switch values are 0/1; press events last at least one input poll, then release. Held switches coalesce until every source releases. Cancel gameplay output while paused/unfocused or tracking is invalid.

| Inputs | Raw values / direction |
|---|---|
| Steering | 0 left,128 centre,255 right; full wheel ±135°, wrist option ±45°; stick ±1 |
| Accelerator, Brake, RearBrake | 0 released to255 full squeeze; trigger deadzone/curve in the element |
| GearShift1–4, GearShiftN | Four exclusive gates plus neutral; latch on release; no lateral gear crossing outside neutral |
| GearShiftUp, GearShiftDown | One pulse on lever/stick entering each detent, release at centre |
| VR1–4, ViewChange, HandBrake, MusicSelect, Start1/2, Coin1/2 | Exact switches; view selector uses right grip released, gear buttons right grip held |
| AnalogGunX/Y and X2/Y2 | X0 left/255 right; Y255 up/0 down, centre128; independent player slots |
| SkiX, SkiY | X255 left/0 right; Y0 forward/255 back; apply each explicit invert once |
| MagicalLever1/2, MagicalPedal1/2 | Lever255 up/0 down, centre128; pedals are switches at trigger threshold0.55 |

Place one original GLB per `model_instance`, not one copy per named button/axis. Shared parts use one root position per player. Cabinet assets are in `assets/controls/`, with motion tables in `data/controls/models/`; reused guns keep `assets/guns/` and `data/guns/`. Metres,+Y up,−Z forward; draw one LOD. Resolve the selected node and its LOD alias, and use its current transform for labels and moving pieces. Angle ranges are radians; slides metres. Native port drives normalize0..1. `gate_x/z` are the snapped knob coordinates; sequential `shift` is0 back/down,0.5 centre,1 forward/up. Wheel regrab preserves the current angle; release springs at the configured rate. `tools/cabinet_motion.py` contains tested wheel, gate and pedal maths.

Pistols need a grip-position/aim-orientation transform and parallax-correct scene hit projected through the original game camera. Convert viewport Y to upward raw gun counts once. Mounted guns use a seat-anchored calibrated yaw/pitch/lift range; do not replace their ADC mapping with a free pistol ray. The two-gun hold has explicit separate players/ports: Start2 joins player2, not a grip gesture. Recoil must not move the input aim. Body lean/rock is offered only when the player selects its pictured card before launch.

Limits: Harley has no Clutch port; Le Mans/Dirt Devils/ECA expose sequential shift, not a direct H gate; Dirt Devils has no HandBrake port; Magical Truck pedals cannot express analog pressure. Analog-gun games have no enabled `Offscreen` port, so no invented reload button is shipped; precise second-trigger captions remain questions. Cabinet force feedback, platform motors and a distinct motorcycle roll axis are not represented here. Models are original approximate dimensions, not measurements of the real cabinets. These deliverables are models, data, vector pictures and reference maths; the live emulator's input/hand-render consumer and controller feel have not been exercised by this task.
