# Wii motion games on Quest Touch — DolphinXR mapping

Explicit Wii Remote and Nunchuk bindings for the five Wii motion games (Wii Sports Resort,
Mario Kart Wii, Punch-Out!!, Super Mario Galaxy, Zelda: Twilight Princess) on the pinned
DolphinXR fork, plus the anti-aliasing fixes found while checking the owner's first headset run.

## How DolphinXR feeds the emulated Wii Remote (source reading, pinned fork)

- `Source = 3` in `WiimoteNew.ini` is `WiimoteSource::OpenXR` (`Core/HW/Wiimote.h`). It is an
  emulated Wii Remote with an **input override** installed (`Core/HW/Wiimote.cpp`,
  `CreateOpenXRInputOverrideFunction`).
- The override supplies only motion and pointing: the Wii Remote's accelerometer, gyroscope
  (MotionPlus) and IR pointer come from the right controller (aim pose orientation, grip
  velocity, aim-ray hit on the virtual screen); the attached extension (Nunchuk) gets the left
  controller's accelerometer. It returns nothing for buttons, D-pad or sticks, and it is only
  active while an OpenXR session is running.
- Buttons and sticks go through ordinary Dolphin bindings on the device
  `OpenXR/0/OpenXR Controller` (`InputCommon/ControllerInterface/OpenXR/OpenXR.cpp`). Its inputs
  per hand: `Left|Right Button X|Y` / `A|B`, `Button Menu`, `Button Trigger`, `Button Squeeze`,
  `Button Thumbstick`, analog `Trigger`, `Squeeze`, `Squeeze Force`, `Thumbstick X+|X-|Y+|Y-`, raw
  accel/gyro inputs, and outputs `Motor`, `Motor Left`, `Motor Right`.
- With no binding lines Dolphin loads an empty expression for every button (`ControlGroup::LoadConfig`
  defaults to ""). The stock profile `Sys/Profiles/Wiimote/OpenXR Wii Remote.ini` is applied only when
  the mapping window is opened in the desktop GUI. So the previous `UserMotion` file (Source,
  Device, Extension only) gave the games **no buttons at all**; only pointer and motion worked.
- On Quest Touch the fork binds `Right Button Menu` to the right controller's system button, which
  the runtime normally keeps for itself, so the mapping uses only the left Menu.

## The mapping

`WiimoteNew.ini` (generic) and the profile `AC Motion Remote and Nunchuk`:

| Wii | Quest Touch |
| --- | --- |
| Pointer, tilt, swing, MotionPlus | right controller itself (override) |
| A | right A |
| B | right trigger |
| 1 | right B |
| 2 | right stick click |
| D-pad | right stick directions |
| + | left Menu (not while left Y is held) |
| − | left stick click |
| Home | hold left Y and left Menu together for 0.5 s |
| Nunchuk stick | left stick |
| Nunchuk C | left X |
| Nunchuk Z | left trigger |
| Nunchuk motion | left controller itself (override) |
| Rumble | right controller |

Profile `AC Motion Wheel` (Mario Kart Wii): no Nunchuk, so the game uses its sideways/wheel
scheme and steering is the right controller's roll. 2 = right trigger (accelerate), 1 = right B,
B = right grip or left trigger (drift), D-pad = either stick (items), A = right A, + − Home as above.
With a Nunchuk attached Mario Kart Wii switches to stick steering, which is why this game gets its
own set.

Profile `AC Motion Resort` (Wii Sports Resort): the generic set, but the Nunchuk starts detached and
**left Y + left stick click** attaches or detaches it (`Extension = toggle(...)`; − is gated off
while Y is held). Offline, Resort with a Nunchuk attached stopped on a screen asking to remove the
extension from the MotionPlus; without it the game went on to its intro.

| Game | ID | Set |
| --- | --- | --- |
| Wii Sports Resort | RZTP01 | AC Motion Resort |
| Mario Kart Wii | RMCP01 | AC Motion Wheel (+ `MSAA = 4`) |
| Punch-Out!! | R7PE01 | AC Motion Remote and Nunchuk |
| Super Mario Galaxy | RMGP01 | AC Motion Remote and Nunchuk |
| Zelda: Twilight Princess | RZDP01 | AC Motion Remote and Nunchuk |

Per-game sets are chosen by `GameSettings/<ID>.ini` → `[Controls] WiimoteProfile1 = <profile>`
(`InputCommon/InputConfig.cpp`), profiles in `Config/Profiles/Wiimote/`. Every Wii button is
reachable in both sets; `check_bindings.py` verifies that and that every input name exists on the
device.

The wrist-strap/health screen takes **A** (no pointing needed). Super Mario Galaxy's and
Twilight Princess's title screens ask for **A + B** together (right A + right trigger).

## Anti-aliasing and internal resolution in the VR path

- With OpenXR on, `stereo_mode` becomes `OpenXR` and the EFB is a two-layer render target of
  `640×528 × InternalResolution` with `MSAA` samples (`VideoCommon/FramebufferManager.cpp`);
  the eye images are blitted from the XFB copied out of that EFB (`Presenter::BlitCurrentSourceToOpenXREyes`).
  So `InternalResolution`, `MSAA`/`SSAA` and anisotropic filtering do apply to what the headset shows;
  `VR/ResolutionScale` only sizes the swapchains.
- **Game INIs beat the command line.** Dolphin searches LocalGame, then GlobalGame, then CommandLine
  (`Common/Config/Enums.h`). The shipped `Sys/GameSettings/RMC.ini` sets `[Video_Settings] MSAA = 0`,
  so `-C Graphics.Settings.MSAA=4` never reached Mario Kart Wii. `GameSettings/RMCP01.ini` here sets
  `MSAA = 4` at the LocalGame layer.
- **Anisotropy key.** Dolphin reads `Graphics.Enhancements.MaxAnisotropy` (value 4 = 16x);
  `Graphics.Settings.MaxAnisotropy` is not read by anything.

## Offline input probe (test only)

`input-probe/` adds an env-gated probe to a private source clone (`apply.py <source>`,
`--revert` to remove). `DOLPHINXR_INPUT_SCRIPT` feeds a scripted controller state keyed on
emulated time into the same OpenXR input state the real session fills, and
`DOLPHINXR_INPUT_PROBE` logs what emulated Wii Remote 1 reports (buttons, accelerometer, IR dot,
MotionPlus, Nunchuk). `run_probe.ps1` runs one hidden, below-normal, time-limited `DolphinNoGUI -p headless`
with OpenXR off and `XR_RUNTIME_JSON` pointed at a missing file. Scripts: `bindings-*.txt`.

## Install (private user folder)

Copy `WiimoteNew.ini` to `<user>/Config/`, `Profiles/Wiimote/*.ini` to
`<user>/Config/Profiles/Wiimote/`, `GameSettings/*.ini` to `<user>/GameSettings/`, keeping a
backup of anything replaced. Launchers add
`-C Graphics.Settings.InternalResolution=3 -C Graphics.Settings.MSAA=4 -C Graphics.Settings.SSAA=False -C Graphics.Enhancements.MaxAnisotropy=4`.

## Offline results (2026-10-19, hidden DolphinNoGUI, OpenXR off, no XR runtime)

- Every binding of all three sets reached emulated Wii Remote 1 as the right report bit or value:
  A, B, 1, 2, −, +, Home (only after the 0.5 s hold, and + stayed off during it), the four D-pad
  directions from either stick where mapped, Nunchuk C, Z and all four stick directions, Mario Kart's
  grip/left-trigger B and trigger 2, the Resort Nunchuk toggle (attached, then detached, − stayed off).
- The override paths also arrived: right roll/pitch changed the Wii Remote accelerometer, the aim
  hit moved the IR dot, right angular velocity moved the MotionPlus gyro, left roll/pitch changed the
  Nunchuk accelerometer.
- Right A skipped the wrist-strap/health screen in Mario Kart Wii, Super Mario Galaxy, Twilight Princess,
  Wii Sports Resort and Punch-Out!! (pictures differ from a no-input control run at the same emulated
  time). In Mario Kart Wii, A on the title, 2 and A again reached the Mii selection; in Galaxy,
  A + B on the title reached the file select.
- Dolphin's own log at EFB creation: Mario Kart Wii with only `-C Graphics.Settings.MSAA=4` → `msaa=1`;
  with `GameSettings/RMCP01.ini` → `msaa=4`; Super Mario Galaxy with the command line only → `msaa=4`.
  Screenshots at `InternalResolution=3` are 2501×1368 against 834×456 at 1x.
- Not proven offline: the stereo OpenXR render itself (the runs are mono), anisotropic filtering, and
  anything in the headset.
