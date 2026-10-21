# GameCube racers on Quest Touch â€” DolphinXR mapping

GameCube discs run on the same pinned DolphinXR build as the Wii games. This folder holds the
GameCube pad mapping for Quest Touch (with steering by rolling the right controller), the per-game
settings, and an offline probe that proves the bindings reach the emulated GameCube pad.

## How DolphinXR feeds a GameCube pad (source reading, pinned fork)

- There is no GameCube counterpart of the Wii Remote's OpenXR override (`WiimoteSource::OpenXR`).
  An emulated GameCube pad is an ordinary Dolphin controller bound to the device
  `OpenXR/0/OpenXR Controller` (`InputCommon/ControllerInterface/OpenXR/OpenXR.cpp`).
- That device exposes, per hand, the buttons (`Right Button A`, `Left Button X`, `â€¦ Menu`,
  `â€¦ Trigger`, `â€¦ Squeeze`, `â€¦ Thumbstick`), analog `Trigger`, `Squeeze`, stick half-axes and
  **`Raw Accel Up/Down/Left/Right/Forward/Backward`**: the gravity direction in the controller's own
  frame, computed from the grip pose (`UpdateMotionState`), scaled so 1.0 = 1 g. Rolling the
  controller moves gravity onto its x axis, so `Right Raw Accel Left/Right` = sin(roll). That is
  the pose input used for steering. It is only valid while an OpenXR session reports a pose.
- Dolphin ships `Sys/Profiles/GCPad/Quest Touch GameCube.ini`, applied only from the desktop mapping
  window, so a fresh user folder has no GameCube bindings at all. `GCPadNew.ini` here supplies them.

## The mapping: `AC GC Racer` (default for the whole `UserGC` folder)

Owner decision (2026-10-20): roll steering is an opt-in, off by default. The default set steers with
the left stick; `AC GC Racer Roll Steer` (`GCPadNew-roll-steer.ini`, installed as the separate user
folder `UserGCRoll` with "(roll steer)" launchers that share `UserGC`'s memory card) adds roll.

| GameCube | Quest Touch |
| --- | --- |
| Control stick | left stick (roll-steer set only: rolling the right controller also steers, 10Â° dead zone, full lock at about 37Â°) |
| A (accelerate in all three racers) | **right trigger** or right A |
| B (brake / reverse; crouch in Wave Race) | **left trigger** or right B |
| X / Y | left X / left Y |
| Z | right stick click |
| Start | left stick click (not while the stick is tilted: click and tilt is the D-pad) |
| L / R (analog and click) | left grip / right grip |
| C stick | right stick |
| D-pad | hold left stick click and push the left stick (the control stick is held off meanwhile) |
| Rumble | right controller |

Roll-steer expression (Dolphin's own expression language, no code change):
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

Probe runs must pass `-C Dolphin.Interface.UsePanicHandlers=False -C Dolphin.Interface.OnScreenDisplayMessages=False`
(and the user folders carry the same under `Dolphin.ini [Interface]`): DolphinNoGUI's default alert handler is a
real Windows `MessageBox`. With panic handlers off, warnings are only logged (at error level, under the caller's log
type); questions and critical alerts still open a box, so keep logs on to see them.

## Launching (private)

No `.cmd` files. The Hub reads `.local/play/gamecube/games.json` (key = Hub game id, views in order: `3d` first,
then the opt-in `tilt-steer`; each view has its user folder, exact Dolphin arguments and env) and starts
`.local/play/gamecube/launch-gamecube.ps1 <key> -View <view> [-NoXr] [-PrintOnly]` (exit 2 unknown key/view,
3 no SteamVR, 5 game guard refused). `-NoXr` is the offline path (DolphinNoGUI headless, hidden, below normal,
OpenXR off, missing XR runtime); with `AC_GC_PROOF_EXE`, `AC_GC_PROOF_USER`, `AC_GC_PROOF_UNTIL` and the probe
variables it runs the proofs below through the exact start path.

## Offline results (2026-10-20, F-Zero GX GFZE01, USA disc)

- Bindings (`input-probe/bindings-gc-racer.txt`): every Quest input reached GameCube pad 1 as the right bit or value:
  A from right A and from the right trigger, B from right B and from the left trigger, X, Y, Z, Start, the four
  control-stick directions (0x01 / 0xFF), C stick, L and R (digital bit + analog 255 from the grips; 40 % grip gives
  analog 102 with no click), the four D-pad directions with the control stick held at centre. Roll-steer set: 8Â° roll
  gives nothing (dead zone), Â±20Â° gives 80 / 176, Â±40Â° and Â±60Â° full lock, roll left steers left.
- Boot to play (`fzero-*.txt`): save file created, name entered and registered on the card, title (Start), Grand
  Prix, Novice, Ruby Cup, Blue Falcon, machine settings, Mute City race running with the timer counting.
- Steering (`fzero-steer-*.txt`, right trigger held from 78 s, variant input 88â€“98 s): with no input the machine
  sits against the wall of the first turn (42â€“74 km/h); two no-input runs show the same speeds and lap times at
  every picture. Left-stick left or left-stick right drives the machine off the wall (207â€“643 km/h, different picture for
  left and right); left trigger brings it to 0 km/h. The first picture of the brake run (86 s, before its input)
  already differs slightly (725 vs 782 km/h), so emulation or picture timing is not exactly repeatable.
- Dolphin's log shows `msaa=4` at EFB creation; pictures are 3x internal resolution. The shipped DolphinXR VR profile
  `GameSettingsVR/GFZE01.ini` loads (8 element overrides, 1 shader override). The stereo OpenXR view itself and any
  picture beyond the 4:3 frame were not checked offline.
- Through the Hub's start path (`launch-gamecube.ps1 gc-f-zero-gx -View 3d -NoXr` and `-View tilt-steer -NoXr`, probe
  build, test user folders): the left-stick-left run repeated the earlier left run's speeds exactly (569 / 469 /
  477 km/h at 90 / 94 / 98 s); rolling the right controller 30° left (tilt-steer view) put the control stick at 36
  (left) and the race picture left the wall like the stick run, against the no-input run. All four runs logged `msaa=4`.
  The only errors logged were `CreateDir` failures for a deleted shader-cache folder; no alert or question fired.
