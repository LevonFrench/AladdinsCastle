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
| X / Y | left X / left Y (alone) |
| Z | left X and left Y together (the stick clicks belong to the universal layer; X and Y alone stay X and Y) |
| Start (U1) | left stick click alone (150 ms chord wait, see `../motion-controls/README.md`); the right click is U2 Select and does nothing on GameCube |
| Recentre (U3), quit and save (U4) | both clicks (released within 1 s) and both clicks held 3 s, through the Dolphin hotkeys in `../templates/Hotkeys.ini` |
| L / R (analog and click) | left grip / right grip |
| C stick | right stick |
| D-pad | not bound: the clicks are no longer modifiers and no racer needs it (menus take the control stick) |
| Rumble | right controller |

Roll-steer expression (Dolphin's own expression language, no code change):
`Main Stick/Right = max(`Left Thumbstick X+`, clamp((`Right Raw Accel Left` - 0.17) * 2.2, 0, 1))`.

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

## More GameCube games (2026-10-26, offline only)

Nine more discs were added to the private launcher next to F-Zero GX, each with one `3d` view: Mario Kart: Double Dash!!, Kirby Air Ride, Star Wars Rogue Squadron II: Rogue Leader, Super Mario Sunshine, Super Monkey Ball, The Wind Waker, Pikmin, Luigi's Mansion and Twilight Princess. Each has a catalog entry under `games/gc-*/` whose id comes from the public disc index (`data/wii-disc-index.json`): the Hub matches the game ID in the disc header (for example `GM4E01`), never the file name. Every view uses 5x internal resolution, MSAA 4x and 16x anisotropic filtering, and DolphinXR's shipped VR profile for the game (`Sys/GameSettingsVR/<game id>.ini`, present for all nine) is picked up by game ID, as it was for F-Zero GX (log lines were not captured in these runs, so the profile loading is expected, not re-proven). No per-game settings file was added.

How each was checked (probe build, hidden, muted, OpenXR off, scripted pad input, Dolphin's own frame dumps; speed with Dolphin's side-by-side stereo so both eyes are drawn, uncapped, as times real time):

| Game | Reached | Speed in play |
|---|---|---|
| Mario Kart: Double Dash!! | Grand Prix Mushroom Cup race, gas and steering change the picture | 4.6 to 7x |
| Kirby Air Ride | Top Ride three-lap race, steering | 5.3 to 5.6x |
| Rogue Leader | Death Star Attack flight, steering | 1.9x at the busiest moment, 4 to 9x elsewhere |
| Super Mario Sunshine | file created, Delfino Airstrip, Mario walks and swims | 5.7 to 6.7x |
| Super Monkey Ball | Beginner Floor 1 to the goal, Floor 2 started | 4.1 to 6x |
| The Wind Waker | save prompt answered Yes, legend, Outset Island: Link under control on the lookout with HUD and Speak prompt | 5.5 to 6.7x |
| Pikmin | Olimar walking the field | 9.3 to 11x |
| Luigi's Mansion | Luigi in the Foyer, walking | 4.5 to 5.3x |
| Twilight Princess | save created, Ordon intro, Link walking the village | 5.4 to 9x |

Twilight Princess and The Wind Waker ask "create a save file?" on first start with the answer set to No. Pressing A repeatedly picks No, then "Continue without saving?" with No again, which loops forever. Push the stick left to choose Yes, then press A. (The earlier stall in the lane report was this, plus a raw test card.)

Super Monkey Ball needs a Start press at the title. The `launch-gamecube.ps1 <key> -View 3d -NoXr` start path was run for all nine with the probe build and a test user folder (real time, to a boot checkpoint each). Not headset-verified. The D-pad is not mapped in the universal layout, so Rogue Leader's wingman commands are unavailable.
