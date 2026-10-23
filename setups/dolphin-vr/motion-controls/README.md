# Wii motion games on Quest Touch — DolphinXR mapping

Explicit Wii Remote and Nunchuk bindings for the five Wii motion games (Wii Sports Resort,
Mario Kart Wii, Punch-Out!!, Super Mario Galaxy, Zelda: Twilight Princess) on the pinned
DolphinXR fork, plus the anti-aliasing fixes found while checking the owner's first headset run.

## Current audit and evidence limits — 2026-10-23

The [scorecard](../scorecard/README.md) separates current configured bindings, constructed
emulated reports, visible game actions and physical-controller acceptance. The older results
below retain their original game state, seed and method; they are not renewed acceptance of
every launcher view. A static binding check proves neither that a game selected that profile
nor that its action occurred. Inspect report transitions during presses, releases and the
Home hold threshold; the parser's latest preceding state can be stale.

The current 19 default-profile observations reached their bounded endpoints:
18 logged no FIFO errors; Excitebike logged 19 and remains failed. Actual report
transitions confirm Plus, Minus and Home, including a side effect: holding Y first
sends Minus, then Home. The configured hold uses 0.5 wall seconds; probe timestamps
use emulated time. Stick and motion report changes do not establish physical dead
zones, natural gesture strength or successful game actions. Twelve pointer-game
observations still lack a validated numeric accuracy result; some stop at menus,
logos or titles and do not supply nine usable cursor points.

For aim, inspect all nine actual cursor hotspots and validate the game-coordinate viewport
before measuring. Synthetic captures can be stereo pairs, mono, or a room panel surrounded by
other graphics. The legacy detectors expect one game viewport; a successful detector exit or
a two-layer texture does not validate that geometry. Keep rejected detections and missing
points explicit. Scripted screen hits and motion inputs do not measure tracking, hand-eye
alignment, natural shake strength, comfort or haptics.

The three binding replays now use a short left-Y tap for Minus and leave the
long-Y Home test free of reserved Menu input. Five play-route candidates
(`galaxy2-play`, `kirbyrtdl-play`, `kirbyyarn-play`, `nsmbw-play`, `snp2-play`)
now send their intended Plus pulses through left-stick click. Those pulses may
pause after a story ends, so the changed routes require fresh scene review;
earlier gameplay receipts remain tied to the original scripts. Neither reserved
Menu button is used by these corrected replays.

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
  the runtime normally keeps for itself. Through ALVR on Quest the left Menu opens the SteamVR
  dashboard (owner, 2026-10-21: no way to press + or Start), so the mapping uses neither Menu button.

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
| + | left stick click (not while left Y is held) |
| − | tap left Y (Minus is active for the first half second of a press, so holding Y for Home sends a brief Minus first) |
| Home | hold left Y for 0.5 s (a tap of left Y is −) |
| Nunchuk stick | left stick |
| Nunchuk C | left X |
| Nunchuk Z | left trigger |
| Nunchuk motion | left controller itself (override) |
| Rumble | right controller |

Mario Kart Wii defaults to `AC Motion Kart` (Wii Remote + Nunchuk, steering on the left stick; controller tilt does not steer), the owner's choice of 2026-10-20. Tilt steering is opt-in: profile `AC Motion Wheel` (no Nunchuk, so the game uses its wheel scheme; 2 = right trigger, 1 = right B, B = right grip or left trigger, D-pad = either stick) through a separate "(tilt steer)" launcher. Both launchers write `GameSettings/RMCP01.ini` before starting (`GameSettings/RMCP01.ini` and `RMCP01-tilt-steer.ini` here), because the game INI picks the profile. Any other racer set up later defaults to stick steering too.

Profile `AC Motion Resort` (Wii Sports Resort): the generic set, but the Nunchuk starts detached and
**left Y + left stick click** attaches or detaches it (`Extension = toggle(...)`; − is gated off
while Y is held). Offline, Resort with a Nunchuk attached stopped on a screen asking to remove the
extension from the MotionPlus; without it the game went on to its intro.

| Game | ID | Set |
| --- | --- | --- |
| Wii Sports Resort | RZTP01 | AC Motion Resort |
| Mario Kart Wii | RMCP01 | AC Motion Kart (+ `MSAA = 4`); AC Motion Wheel only in the tilt-steer launcher |
| Punch-Out!! | R7PE01 | AC Motion Remote and Nunchuk |
| Super Mario Galaxy | RMGP01 | AC Motion Remote and Nunchuk |
| Zelda: Twilight Princess | RZDP01 | AC Motion Remote and Nunchuk |

Per-game sets are chosen by `GameSettings/<ID>.ini` → `[Controls] WiimoteProfile1 = <profile>`
(`InputCommon/InputConfig.cpp`), profiles in `Config/Profiles/Wiimote/`. Every Wii button is
reachable in both sets; `check_bindings.py` verifies that and that every input name exists on the
device.

The wrist-strap/health screen takes **A** (no pointing needed). Super Mario Galaxy's and
Twilight Princess's title screens ask for **A + B** together (right A + right trigger).

## Wii gun games (separate user folder)

The gun games' `WiimoteNew.ini` had the same gap (Source and Device only). `gun/WiimoteNew.ini`
is the default for that user folder and `gun/<ID>.controls.ini` holds the `[Controls]` block to
append to each game's existing private `GameSettings/<ID>.ini` (those already carry frame-rate codes;
keep them). Every set maps every Wii button the generic way (B = right trigger, A = right A,
1 = right B, 2 = right stick click, D-pad = right stick, + = left stick click, − = tap left Y,
Home = hold left Y, Nunchuk stick/C/Z = left stick/left X/left trigger) plus the right
grip as a second-finger button where the game's reload or action sits:

| Game | ID | Set | Right grip | Roles from reviews (no manual found) |
| --- | --- | --- | --- | --- |
| Resident Evil: The Umbrella Chronicles | RBUP08 | AC Gun Umbrella Chronicles | A | B fires, shake the Remote reloads, A interacts, Nunchuk stick looks |
| Resident Evil: The Darkside Chronicles | SBDP08 | AC Gun Darkside Chronicles | A | B fires, A picks up, D-pad or Nunchuk stick changes weapon |
| The House of the Dead: Overkill | RHOP8P | AC Gun House of the Dead Overkill | A | B fires, A or a shake reloads, 1/2 change weapon |
| Dead Space: Extraction | RZJE69 | AC Gun Dead Space Extraction | Z | B fires, Z reloads, C stasis, A kinesis, tilt for alt fire |
| Sin & Punishment: Star Successor | R2VE01 | AC Gun Sin and Punishment | A | B fires, C jump, Z evade, A charge shot (remappable in game) |

Shake-to-reload is physical: with `Source = 3` the override replaces the whole accelerometer, so a
button bound to Dolphin's Shake group would have no effect; shaking the right controller itself does.
None of the five shipped game INIs set MSAA; the gun launchers had no resolution or AA flags at all
and now pass the same four `-C` settings as the motion launchers.

### Aim of the games the owner already plays (2026-10-20)

Same method, nine points at ±0.45, measured on each game's own cursor or reticle with the launcher's
graphics settings; corrections are in each game's button set.

| Game | Cursor used | Before (max error) | Trim (Yaw, Pitch, Offset cm) | After (max error) |
| --- | --- | --- | --- | --- |
| Umbrella Chronicles RBUP08 | red reticle, save prompt | 24% (1.62x both axes, 11% high) | 15.8, 11.4, 15 | 1.3% (mark sheet confirms all nine) |
| Darkside Chronicles SBDP08 | red reticle, save prompt | 24% (1.21x, 1.25x, 18% low) | 20.7, 15.7, 0 | 0.6% |
| House of the Dead Overkill RHOP8P | grey ring, save prompt | about 10% (1.31x wide, 7% high; read from pictures) | 18.75, 19, 15 | centred at nine points by eye (mark sheet) |
| Dead Space Extraction RZJE69 | crosshair lines, title | 8.5% (1.36x wide, 1.05x tall, 7.6% high) | 18.2, 19.2, 15 | 0.5% |
| Sin & Punishment SoS R2VE01 | gecko, keys | 1.32x wide | 18.75, 19.5, 18 | keys hit |
| Super Mario Galaxy RMGP01 | star of the star pointer | 8.6% (1.32x wide) | 18.9, 19, 11 (set AC Motion Galaxy) | 1.2% |
| Zelda Twilight Princess RZDP01 | blue reticle, save screen | 24% (1.0x wide, 1.3x+ tall, 22% low) | 25, 13.2, 0 (set AC Motion Zelda) | 1.0% |
| Wii Sports Resort RZTP01 | fingertip of the hand | 10% (1.31x wide, 9% high) | 19, 19, 16 (set AC Motion Resort) | 0.4% |

Punch-Out!! and Mario Kart Wii (wheel) have no pointer in play and keep Dolphin's defaults.

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

## Stereo in the headset: the virtual screen (2026-10-21)

Measured offline with per-eye pictures from `../stereo-probe/` (the real DolphinXR stereo path with a
synthetic head, no OpenXR runtime). E2 only; nothing here was seen in the headset.

- **`[VR] VirtualScreen` decides where 2D goes, not whether the world is 3D.** The 3D world is drawn
  per eye either way. With `VirtualScreen = False` every orthographic draw (title, menus, HUD,
  crosshair, strap screens) keeps the game's own projection: both eyes get the identical picture
  (mean left/right difference 0.0, disparity 0 px), it fills the view and it turns with the head
  (a 20° synthetic head turn left it on the same pixels). With `True` the same draws sit on a
  screen in the room: 15 px disparity at `ScreenDistance = 4`, 41 px at 1.5 m (expected
  0.064 m / distance × 960 px for a 1920-pixel eye), and they stay put when the head turns.
- **The per-game VR profile wins** over `Config/GFX.ini` (`GameSettingsVR/<ID>.ini` is applied at the
  CurrentRun layer). Before this fix every Wii game we launch ended with `VirtualScreen = False`
  except Mario Kart Wii (DolphinXR's shipped `RMCP01` profile sets `True`), the one game with no
  stereo complaint. The gun-game profiles had copied Umbrella's `VirtualScreen = False` without
  Umbrella's HUD shader overrides, so their 2D was glued to the face.
- **`True` is not right for every game.** Games that build their picture from full-screen 2D passes
  get a flat box ("inner window") with it on: Wii Sports Resort, Twilight Princess, Galaxy 2,
  Kirby's Return to Dream Land (title screens), Sin & Punishment (stale, displaced layers), and
  Umbrella's mission (2026-10-11). So the folder defaults stay as they were and the setting is
  per game:

| Game | ID | VirtualScreen | Evidence (offline, 2026-10-21) |
| --- | --- | --- | --- |
| Dead Space: Extraction | RZJE69 | **True** (`gun/GameSettingsVR/`) | profile menu: 0 px → 15 px, stays in the room on a 20° turn |
| House of the Dead: Overkill | RHOP8P | **True** (`gun/GameSettingsVR/`) | title: 0 px → 15 px |
| Gunblade NY & L.A. Machineguns | SQDE8P | **True** (`gun/GameSettingsVR/`) | name entry: 0 px → 15 px |
| New Super Mario Bros. Wii | SMNE01 | **True** (`GameSettingsVR/`) | world map HUD: 0 px → 41 px |
| Punch-Out!! | R7PE01 | **True** (`GameSettingsVR/`) | save screen on the panel |
| Mario Kart Wii | RMCP01 | True (DolphinXR's profile) | race: road 101 px near, sky 0 px, HUD ~41 px |
| Umbrella, Darkside, Sin & Punishment, Resort, Zelda, Galaxy 1 and 2, Kirby x2, Strikers, DKCR, Metroid | | False (unchanged) | flat box or seam with True, or no gain; needs per-game HUD overrides like Umbrella's |

- **Not the cause:** `InternalResolution=3`, `MSAA=4` and anisotropy. Mario Kart Wii at 3x/MSAA 4 and at
  1x/no MSAA gave the same disparity profile (near road 101 px vs 33 px × 3 = 99 px, sky 0 px), and
  Metroid behaved the same in both. The OpenXR path resolves every EFB layer.
- **Metroid Prime Trilogy (separate fault):** 30 of 48 sampled frames (37 of 48 at 1x) were presented
  from a one-layer XFB. Its shipped `Sys/GameSettings/R3M.ini` sends XFB copies to RAM, the copies are
  640×448 while the video interface asks for 640×480, so the texture cache misses and Dolphin
  rebuilds the frame from RAM as a single layer (with stale "Metroid Prime Loading..." pixels).
  `Presenter::BlitCurrentSourceToOpenXREyes` skips any source with fewer than two layers, so those
  frames never reach the headset. `XFBToTextureEnable = True` did not help (18 of 34 still mono);
  `ImmediateXFBEnable = True` made all 33 frames stereo but showed a corrupt frame and many more
  FIFO errors, so it is not applied. The real fix is in the emulator: build the XFB container with
  the EFB's layer count and stitch each layer.

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
backup of anything replaced. VR profiles: `GameSettingsVR/*.ini` into the motion folder's
`GameSettingsVR/`, `gun/GameSettingsVR/*.ini` into the gun folder's `GameSettingsVR/`. Launchers add
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
- Gun games (2026-10-20, same method, launcher graphics flags): in all five games every Wii Remote
  and Nunchuk binding reached the report, the right grip landed on A (on Z for Dead Space:
  Extraction), Home only after the hold. The wrist-strap and Zapper pages wait for A in all five; with
  right A every 2 s the games left them (save prompt, logos, file select) while the no-input control
  was still on them at 32 s. Dolphin's log shows `msaa=4` in every gun run.
- Not proven offline: the stereo OpenXR render itself (the runs are mono; see the virtual-screen
  section above and `../stereo-probe/` for the later per-eye pictures), anisotropic filtering, and
  anything in the headset.

## More owned games (2026-10-20): pointer aim measured against each game's own cursor

Method (as for the PS1 guns): reach a screen where the game draws its pointer, hold the scripted
aim ray at nine points (centre, corners and edge midpoints at ±0.45), take a picture 1.5 s into
each, find the cursor (`input-probe/measure_cursor.py` for colour cursors, `measure_tip.py` for an
arrow tip) and compare with the aim point `x = (u+1)/2, y = (1-v)/2`. Fit scale and shift per axis
and turn them into the Point group's own settings in the profile: `IR/Total Yaw` (horizontal scale),
`IR/Total Pitch` (vertical scale), `IR/Vertical Offset` (vertical shift; about 1.5% of the screen
height per cm in these games). With Dolphin's defaults (25°, 20°, 10 cm) the cursors were off by up
to 12% of the screen.

| Game | ID | Set | Cursor used | Before | Trim | After |
| --- | --- | --- | --- | --- | --- | --- |
| Gunblade NY & L.A. Machineguns | SQDE8P | AC Gun Gunblade and LA Machineguns | yellow crosshair, ID screen | x 1.33× wide, 7.7% high | Yaw 18.75, Pitch 19, Offset 15 | max 0.5% |
| Sin & Punishment: Star Successor | R2VE01 | AC Gun Sin and Punishment | gecko cursor; on-screen keys | x 1.32× wide | Yaw 18.75, Pitch 19.5, Offset 18 | aimed keys hit (letter A, OK, YES, menu icons) |
| Metroid Prime Trilogy | R3ME01 | AC Pointer Metroid Prime Trilogy | blue arrow tip, slot screen | x 1.10×, y 0.75×, 11% low | Yaw 22.7, Pitch 26.8, Offset 0 | about 1% (tip at the aim mark) |

The trims are for the game's own mapping of the emulated IR camera to its picture; in the headset
the aim ray is measured against DolphinXR's virtual screen, so they hold where that screen shows the
whole game picture. Metroid Prime Trilogy uses the trigger as Wii A (fire, confirm) and right A or
grip as B (jump).

### Nunchuk prompts (2026-10-21, offline proof)

The owner saw a Nunchuk prompt in Metroid Prime Trilogy (2026-10-21) and in Dead Space: Extraction
(earlier). Hidden `DolphinNoGUI` runs with copies of the owner's own settings folders and the input
probe settled both:

- **The extension does reach the games.** Dolphin's log shows `Switching to Extension 1` (Nunchuk) on
  Wii Remote 1 when it links, and the probe shows Nunchuk data in every report. The profile loads
  through the game INI's `[Controls] WiimoteProfile1`; `Source = 3` (OpenXR) behaves like `Source = 1`
  here (the OpenXR override only replaces accelerometer, gyro and pointer values).
- **Dead Space: Extraction** blocks on "A Nunchuk is required for Player 1" only when no Nunchuk is
  attached (profile set to `Extension = None`). With `Extension = Nunchuk` it goes on to Select Profile,
  with MotionPlus on or off. The gun settings folder had no extension at all until 2026-10-20
  (`WiimoteNew.ini` with only `Source` and `Device`), which explains the earlier prompt. The
  "Wii Zapper Use" page that shows a Nunchuk is only a picture; right A passes it.
- **Metroid Prime Trilogy**: after Press A at the title, the Trilogy menu shows "Nunchuk is required."
  for about 3 seconds, then saves and opens Game Select by itself. This happens in every setup tried:
  Nunchuk or none, MotionPlus on or off, `Source = 3` or `1`, the 2026-10-19 test folder, a live
  re-plug before the title, and a minute of waiting on the title. With or without a Nunchuk the frames
  are byte-identical, so the menu does not look at the Nunchuk there; it is not a missing Nunchuk.
  Metroid Prime itself does check: without a Nunchuk it stops on "Please connect a Nunchuk to the
  Wii Remote." for the rest of the run; with `AC Pointer Metroid Prime Trilogy` it plays (Frigate
  Orpheon, walking on the Nunchuk stick, firing, the game's own "Press and hold Z" hint).
- MotionPlus stays off in the Dead Space and Metroid sets: neither game uses it, and both pass with it
  off. It was not the cause either (both also pass with it on).

Scripts: `input-probe/metroid-title-a.txt` (title message), `metroid-gameplay.txt` (slot 1 filled:
menus, cinematic, then walking and firing), `dse-boot.txt` (Dead Space to Select Profile).

### Queue, batch 1 (2026-10-20)

| Game | ID | Launcher type | Set | Offline result |
| --- | --- | --- | --- | --- |
| Super Mario Galaxy 2 | SB4P01 | pointer | AC Motion Galaxy 2 (Galaxy trim) | star within about 1.4% at six points, lower row by eye; file screens, story, first area with Mario walking |
| New Super Mario Bros. Wii | SMNE01 | pad | AC Motion Remote and Nunchuk | intro, world map, level 1-1 with Mario running and jumping |
| Donkey Kong Country Returns | SF8P01 | pad | AC Pad DKCR (2026-10-21; was AC Motion Remote and Nunchuk) | title, map and level 1-1; plays since 2026-10-21 once the hut-start shake is given (see "Donkey Kong Country Returns and Mario Strikers Charged" below) |

`AC Pad Sideways` (sideways Wii Remote, no Nunchuk, Dolphin's Sideways option on so either stick
gives screen directions) is for the sideways games that follow. None of these discs has a shipped
DolphinXR VR profile and none of their game INIs touches MSAA.

### Mario Kart Wii stick steering, offline proof (2026-10-20)

`input-probe/mkw-kart.txt` presses A through the default menu choices into a Time Trial on Luigi
Circuit, then holds A with the Nunchuk stick left (104-110 s) and right (112-118 s); the probe shows
the Nunchuk stick at 0 and 255. The kart turns left, then right, back onto the track.
`mkw-kart-tiltcontrol.txt` is the same run with the right controller rolled ±60° instead of the stick:
the kart does not turn. Tilt steering stays available only through the opt-in launcher.

### Queue, batch 2 (2026-10-20)

| Game | ID | Launcher type | Set | Offline result |
| --- | --- | --- | --- | --- |
| Kirby's Return to Dream Land | SUKE01 | pad (sideways) | AC Pad Sideways | file select, story skipped with +, world map, level 1-1 with Kirby moving and attacking |
| Kirby's Epic Yarn | RK5E01 | pad (sideways) | AC Pad Sideways | save creation, story (+ offers "stop watching"), Patch Land with Kirby walking |
| Mario Strikers Charged | R4QE01 | motion | AC Motion Strikers (2026-10-21; was AC Motion Remote and Nunchuk) | pointer trimmed; menus, captains, sides, stadium and a played match since 2026-10-21 (see "Donkey Kong Country Returns and Mario Strikers Charged" below) |
| Excitebike: World Rally | WWRP (WiiWare) | pad (racer, stick steering) | AC Pad Sideways | profile, name entry, World Tour, training race running |

Excitebike follows the owner's racer rule: steering is on the stick (D-pad lanes); tilt is not used for steering.

### No message boxes (2026-10-20)

DolphinNoGUI registers no alert handler, so Dolphin's default handler shows real Windows message
boxes. `run_probe.ps1` now passes `-C Dolphin.Interface.UsePanicHandlers=False` (panic alerts and
"Ignore and continue?" asserts are logged and answered "continue"; questions and critical errors
still show). The alerts seen in the logged proof runs were `FIFO: Unknown Opcode` video asserts
(`VideoCommon/OpcodeDecoding.h:166`, "Ignore and continue?") in Mario Strikers Charged and
Excitebike: World Rally; runs without a log may have raised more. A dialog left open blocks
emulation, which may explain some stalled offline runs (DKCR at 1-1, slow Metroid runs); not verified.
﻿
## Donkey Kong Country Returns and Mario Strikers Charged (2026-10-21)

Both stopped for a reason found offline (hidden DolphinNoGUI, OpenXR off, no XR runtime, audio off, scripted input;
evidence level E2, nothing seen in the headset yet).

### Donkey Kong Country Returns (SF8P01): level 1-1 waits for a shake

- Scripted A presses get from the title through the file and map into 1-1 ("Jungle Hijinxs"). Level 1-1 then opens
  on DK's hut with a Wii Remote + Nunchuk icon in a star burst and stays there. The game is alive (smoke animates,
  the pause menu opens with +); it is asking for a **shake**. The first shake smashes the hut sign, DK steps out and
  the level starts. The earlier runs "shook" the controllers by rolling them 80 degrees back and forth; with
  `Source = 3` the Wii Remote's accelerometer comes from the tilt and the controller's real acceleration
  (`CreateOpenXRInputOverrideFunction`), and a tilt swing is only about 1 g, which the game does not count.
  It is not a missing button, a MotionPlus problem, a dual-core problem or a PAL60 problem (tried:
  `Dolphin.Core.CPUThread=False`, `Dolphin.Core.PAL60=False` and MotionPlus off; the hut stayed until a shake came).
- Proof: with a Wii Remote that has no OpenXR override (`Source = 1`) and the Shake group bound to a button, the
  same script reaches gameplay (DK runs, jumps, rolls, loses lives, Game Over screen).
- Fix: the owner's launcher view `3d` now passes `-C Wiimote.Wiimote1.Source=1` and uses profile `AC Pad DKCR`
  (via `GameSettings/SF8P01.ini`), which binds **Wii Remote shake to the right grip and Nunchuk shake to the left
  grip** (`Shake/X|Y|Z`, `Nunchuk/Shake/X|Y|Z`). `Source` is a Config setting (`Wiimote.Wiimote1.Source`); a
  `Source` line inside a profile did not take effect, the command line does. DKCR uses no pointer, so nothing is
  lost. The second view `physical-shake` keeps `Source = 3` (profile `AC Pad DKCR Motion`, no grip shake): wave
  the controller; not provable offline.
- With `Source = 3` a button-bound Shake group moves only the IR dot, never the accelerometer (probe: the
  accelerometer stays 512,512,616 while the shake button is held), so a grip shake needs `Source = 1`.
- DKCR refuses a Classic Controller ("please remove the unsupported external extension controller"), so there is
  no pad-only layout. MotionPlus is detached in both DKCR profiles (the game does not use it).
- Scripts: `input-probe/dkcr-grip-shake.txt` (runs the grip shake from the hut, then runs right, jumps and rolls; use
  it with a user folder holding `AC Pad DKCR` and `-C Wiimote.Wiimote1.Source=1`). `dkcr-play.txt`, `dkcr-sideways.txt`
  and `dkcr-wait.txt` are the 2026-10-20 runs that could not get past the hut.

### Mario Strikers Charged (R4QE01): never reached a match because the menus need a trimmed pointer and a click path

- The game is a pointer menu all the way to the kick-off. Its cursor is a blue hand whose **fingertip** is the
  hot spot (the drawn hand hangs about 7% of the screen height below the tip), so the old default trim put the
  tip 7% above and 33% too wide of where the controller pointed and DONE and the pads were missed.
- Trim (profile `AC Motion Strikers`): `IR/Total Yaw = 18.8`, `IR/Total Pitch = 19.7`, `IR/Vertical Offset = 14.8`.
  Nine aim points measured on the VS wheel (`input-probe/aim-strikers.txt`, tip position by the top of the hand, `input-probe/measure_tip_hand.py`):
  eight of nine within 1.1% of the screen of the aim point, the ninth (first move after a page change) 7%.
- The click path, all with right A (Wii A) on the fingertip position: Press A, save prompt (continues), VS wheel
  (top burst = Domination), Domination Options page DONE (bottom centre), **click the HOME monitor, then a captain
  icon**; same for AWAY; DONE; sidekicks DONE; **choose sides: hold A on the Wii Remote icon at the bottom and drag it
  into the HOME box, then DONE**; stadium PLAY NOW; match. The pads on the floor do nothing; `+` on the options page
  only flips to the cheats page. Script: `input-probe/strikers-play.txt` (menus to the kick-off, then Mario runs
  with the stick, sprints with Z, passes with A, shoots with B).
- The offline match: pitch with Mario's team, a Mega Strike cut-scene with its "block the balls, press A" mini-game,
  score and clock. One-off `FIFO: Unknown Opcode` lines (two in one run, none in the others) are logged; with
  panic alerts off the game carried on.

Offline runs are silent: `input-probe/run_probe.ps1` passes `-C Dolphin.DSP.Backend="No Audio Output" -C Dolphin.DSP.Volume=0 -C Dolphin.DSP.Muted=True` (owner request, 2026-10-21).
