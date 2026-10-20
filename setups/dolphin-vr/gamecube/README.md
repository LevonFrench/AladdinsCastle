# GameCube racers on Quest Touch — DolphinXR mapping

GameCube discs run on the same pinned DolphinXR build as the Wii games. This folder holds the
GameCube pad mapping for Quest Touch (with steering by rolling the right controller), the per-game
settings, and an offline probe that proves the bindings reach the emulated GameCube pad.

## How DolphinXR feeds a GameCube pad (source reading, pinned fork)

- There is no GameCube counterpart of the Wii Remote's OpenXR override (`WiimoteSource::OpenXR`).
  An emulated GameCube pad is an ordinary Dolphin controller bound to the device
  `OpenXR/0/OpenXR Controller` (`InputCommon/ControllerInterface/OpenXR/OpenXR.cpp`).
- That device exposes, per hand, the buttons (`Right Button A`, `Left Button X`, `… Menu`,
  `… Trigger`, `… Squeeze`, `… Thumbstick`), analog `Trigger`, `Squeeze`, stick half-axes and
  **`Raw Accel Up/Down/Left/Right/Forward/Backward`**: the gravity direction in the controller's own
  frame, computed from the grip pose (`UpdateMotionState`), scaled so 1.0 = 1 g. Rolling the
  controller moves gravity onto its x axis, so `Right Raw Accel Left/Right` = sin(roll). That is
  the pose input used for steering. It is only valid while an OpenXR session reports a pose.
- Dolphin ships `Sys/Profiles/GCPad/Quest Touch GameCube.ini`, applied only from the desktop mapping
  window, so a fresh user folder has no GameCube bindings at all. `GCPadNew.ini` here supplies them.

## The mapping: `AC GC Racer` (default for the whole `UserGC` folder)

| GameCube | Quest Touch |
| --- | --- |
| Control stick left/right | **roll the right controller** (10° dead zone, full lock at about 37°) or left stick |
| Control stick up/down | left stick |
| A (accelerate in all three racers) | **right trigger** or right A |
| B (brake / reverse; crouch in Wave Race) | **left trigger** or right B |
| X / Y | left X / left Y |
| Z | right stick click |
| Start | left Menu |
| L / R (analog and click) | left grip / right grip |
| C stick | right stick |
| D-pad | hold left stick click and push the left stick (the control stick is held off meanwhile) |
| Rumble | right controller |

Steering expression (Dolphin's own expression language, no code change):
`Main Stick/Right = max(`Left Thumbstick X+` & !`Left Button Thumbstick`, clamp((`Right Raw Accel Left` - 0.17) * 2.2, 0, 1))`.

## Install (private user folder)

`Config/GCPadNew.ini` and `Config/Profiles/GCPad/*.ini` into
`<user>/Config/`, `GameSettings/*.ini` into `<user>/GameSettings/`. The user folder is a copy of the
Wii motion folder's VR settings (`GFX.ini`: OpenXR on, flat screen off, virtual screen off) plus
`Dolphin.ini [Core] SIDevice0 = 6` (standard controller in port 1). Launchers add
`-C Graphics.Settings.InternalResolution=3 -C Graphics.Settings.MSAA=4 -C Graphics.Settings.SSAA=False -C Graphics.Enhancements.MaxAnisotropy=4`.

## Offline probe (test only)

`input-probe/apply_gc.py <source>` adds `ACPadProbe.h` and two guarded calls to a private source
clone (`--revert` removes them); it combines with the Wii Remote probe in
`../motion-controls/input-probe/apply.py`. `DOLPHINXR_INPUT_SCRIPT` feeds a scripted controller
state (same script format as the Wii probe, including `roll`/`pitch`/`yaw` of the pose) into the
OpenXR input state just before the serial interface polls the pads; `DOLPHINXR_INPUT_PROBE` logs
what GameCube pad 1 reports (buttons, sticks, analog L/R). Neither does anything for Wii titles.
Runs use `../motion-controls/input-probe/run_probe.ps1` (hidden, below normal, time-limited,
OpenXR off, `XR_RUNTIME_JSON` pointed at a missing file).
