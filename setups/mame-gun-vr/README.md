# MAME light-gun theatre viewer

GPL-3.0-or-later viewer on the shared publish presenter. The unmodified generic
MAME publisher supplies ACVRMG v4 mono frames and calibrated gun ports. Build
the viewer with `build-viewer.ps1 -OpenXRInclude <existing-header-directory>
-OutputDirectory <private-output> -RunGatePath <headset-marker>`. Existing
MinGW tools are used; nothing is downloaded. CMake is an alternative with
`ACVR_OPENXR_INCLUDE_DIR` pointing to existing headers.

`acvr_mono_frames.h` is the exact BSD-3-Clause wire header from the generic
publisher. The viewer validates magic/version, dimensions, stride and physical
aspect under its named mutex. It never rotates the picture again: cabinet
orientation has already been applied. Pixels, telemetry and input are exchanged
by lock-or-skip; a skipped frame retains the last complete picture.

The shared presenter's separate theatre mode locates the current eye views and
tracked right aim/grip, then draws a 3.2 m flat screen at 2.5 m, the GLB gun and
the aim dot into one opaque projection layer with two depth-tested eye images.
If the gun's TOML has `[motion.recoil]` (a blowback slide, e.g. `arc-light-gun`),
each shot edge kicks that node back by its range for `duration_ms`, holds it, and
returns it over the same time, as the PS1 theatre does. The offline proof also
saves `left/right-recoil.png` with the slide fully back and records
`recoil_pixels` (how many pixels differ from the slide at rest; zero fails).
Each target has the runtime's own recommended dimensions. There are no quad
layers in this mode. Screen height uses physical aspect, including non-square
pixels. Native pixels are integer-prescaled by nearest blit, then sampled with
linear filtering and mipmaps. The shared renderer and GLB/metadata loader come
from this project's PS1 theatre implementation; their single shared copy is in
`libacvr/publish-presenter`. CRT scanlines are optional and off by default.

Right trigger shoots; off-screen trigger reloads through MAME's normal gun
mapping. Right A starts, right B inserts a coin, right grip is button 2. Hold
left Y for three seconds to request normal exit (left B on the alternative
controller profile). Home recentres. Focus loss releases input and hides the
tracked gun; rendering does not require focus. MAME's 250 ms heartbeat expiry
protects against a dead viewer. A stop file requests the same normal MAME exit
as the hold gesture. Vibration is one pulse per trigger edge; it does not claim
the game acknowledged a hit.

`launch-mame-gun.ps1` accepts explicit existing executable, media, gun and
private writable paths. It supports the game keys in its table and keeps all
MAME cfg/NVRAM/state/output files in private directories. It launches both
children hidden at BelowNormal priority. It waits for normal exit before an
owned-process fallback; a forced close is reported and cannot establish saves.
The launcher opens a runtime for play; never use that route for offline proof.

For proof, launch MAME hidden with `MAME_VR_PUBLISH=1`, `MAME_VR_GUN=1` and
`MAME_VR_CROSSHAIR=1`, no video/sound, bounded duration and private writable
paths. Run the viewer with `--pid <publisher-pid> --model <local-glb>
--metadata <local-toml> --seconds <wall-budget> --offline-proof <private-folder>
--script`. This path never starts the presenter or opens an XR loader. Fixed
head/hand poses aim at the picture centre; scripted coin/start/shot/off-screen
and second-button input uses the same v4 mapping as play. At 36 emulated seconds
it saves the original mono image and picture/front/behind/dot views of both
eyes, then requests MAME's clean exit. The JSON records MAME's own calibrated
crosshair, integer scale, occlusion and GL results. Pixel fixtures do not prove
headset tracking, comfort, a successful hit, reload in every game or real saves.

`validate_proof.py <private-capture-directory>` (Pillow/NumPy) compares both
rendered eyes against the original mono picture: upright mapping must correlate
above 0.97 and beat the vertically flipped alternative. Near/behind gun masks
and the projected dot-to-MAME-crosshair distance are checked, allowing the gun
board's half-pixel quantization. `test-viewer.ps1` runs the new mode against
injected API functions and native hidden GL: per-eye recommended dimensions,
actions, focus release, current poses, a no-draw update, and bounded image wait
retry. No test opens a loader or runtime.

The original and shared Model 2 viewer sources remain unchanged. Their existing
build and test scripts still select the same implementations. MAME, the Hub,
catalog, PS1 and other emulator clients are outside this viewer change.

## MAME patch, offline script mode and per-game settings

`mame-gun-vr.patch` is the whole change to MAME 0.289 (9a119fe9): the
publisher/gun module (`src/frontend/mame/acvr_mono.*`, BSD-3-Clause like
MAME's own files), the analog override fix in `ioport.cpp` and an opt-in
tolerance in `romload.cpp`. Build it as a `SUBTARGET=gun` with the driver
sources of the games you own; nothing is downloaded.

Environment switches read by MAME, all off unless set:

- `MAME_VR_AIM=xs,xo,ys,yo[,margin]` per-game trim around the picture centre,
  `u' = 0.5 + (u - 0.5) * xs + xo`, the same convention as the PS1 host's
  `ACVR_PS1_AIM`. A trimmed aim is clamped to the picture; only pointing off
  the screen reads as off-screen. An on-screen aim also stays `margin`
  (default 0.004) inside the field's range: many gun boards report their
  extreme values as off-screen.
- `MAME_VR_DIPS=PORT:MASK:VALUE;...` (hex) sets DIP or configuration fields at
  start, such as a cabinet's control-type switch.
- `MAME_VR_TRACKBALL=kx,ky` drives trackball counters as an absolute position,
  `kx`/`ky` counts across the picture (for trackball shooters; Rambo III was
  tried and does not suit it, as its trackball also scrolls the field).
- `MAME_VR_CROSSHAIR=1|0` shows or hides MAME's crosshair. Hide it where MAME's
  crosshair calibration does not match the game (any trimmed game).
- `MAME_VR_ALLOW_MISSING=a,b` names ROM files the launcher accepts as absent:
  only PLD/PROM dumps the driver never reads, which older sets lack. Each is
  logged as a warning; program, graphics and sound ROMs are never listed.
- `MAME_VR_SCRIPT=<file>` with `MAME_VR_SNAPDIR=<dir>`: offline proof mode.
  Input comes from a schedule keyed by emulated frame instead of shared
  memory, through the same aim mapping as play, so a run is repeatable to
  the pixel. Commands: `aim p u v`, `off p`, `hold p TBSC n`,
  `press TOKEN n` (any MAME input token, e.g. `SERVICE`), `dip port mask value`,
  `snap name` (PNG of the exact picture the viewer gets plus a log line with
  the field values) and `quit`.

A field with no crosshair calibration and `PORT_REVERSE` is driven reversed,
like MAME's own input would be (Operation Wolf 3 X).

Aim is measured against each game's own cursor or hit marker (accuracy screen,
in-game sight), not against MAME's crosshair:

| Key | Game's own reference | Setting |
|---|---|---|
| `timecris` | Gun Initialize [Standard] accuracy screen, 9 points, max error 0.0052 | none; grip = foot pedal |
| `opwolf3` | in-game sight, 11 points, 0.0058 after trim | `0.7143,0,0.5001,0` plus the game's own boot calibration (service switch at power-on, targets top-right then bottom-left); the game's gun gain is fixed, so the trim sets scale and the calibration sets offset |
| `zombraid` | in-game sight on the name-entry screen, 11 points, max error 0.0068 | none; reload is the pump (grip), not an off-screen shot |
| `rchase` | in-game sight, 14 points; within 0.003 on the playfield | `1.0768,0.0007,1.3296,-0.0700,0.04`; the game maps the gun's whole y range to about 0.18-0.93 of the picture and reads the gun's extreme values as off-screen, hence the 0.04 edge margin; the HUD band above the playfield is out of the sight's reach, as on the cabinet |
| `term2` | in-game sight, 11 points, 0.0039 after trim | `0.9726,0.0032,0.9623,0.0033` after the game's own three-target gun calibration (first boot; the launcher's NVRAM is seeded) |
| `le2` | the game's hit sparks, 0.0038 after trim | `1,0.005,1,0.011`; off-screen trigger reloads |
| `lethalen` | the game's hit sparks, 6 points within 0.0073 | none |
| `policetr13` | the game's red hit circle, 9 points within 0.0005 after trim | `1,0.0065,0.9597,-0.0178` (MAME's crosshair for this set reads about 4% tall) |
| `alien3` | in-game sight, 11 points, 0.0047 after trim | `1,0,1.1789,-0.0012`; mounted guns read as AD sticks |
| `rapidfir` | the game's hit spark, 11 points within 0.0063 after trim | `1,0,1,-0.0125` |
| `crossbow`, `cheyenne`, `chiller`, `claypign`, `combat`, `hitnmiss`, `whodunit8` | the game's own 4x4 shot dot (Exidy 440), 2-11 clean points each, all within 0.0062 | none; NVRAM seeded past the first-boot "E2 PROM initialized" screen |

## Open frame (Namco System 22 / Super System 22)

`ACVR_S22_WIDE=<pixels per side>` (107 = 16:9 for the 640x480 screen) draws the
game's 3D scene a second time into the two added side strips of a wider
picture: the same projection with the screen centre moved out, the game's
full-screen window opened to the new edges, and the side strips taking the
background colour, fog, fade and gamma of the normal mix. The finished 4:3
screen (3D, sprites and the text-layer HUD) sits unchanged in the middle, so
the HUD stays in the 4:3 centre. MAME's renderer does no culling outside 4:3
(only backface culling and near-plane clipping); what reaches the sides is
whatever the game's own code sends. The publisher sends the wider picture
with its physical aspect, the viewer keeps a 4:3 picture 2.4 m tall and adds
width at the sides, and a gun aimed at the added sides reads as off-screen
(the `aimw` script command and the viewer path use the same conversion).
`launch-mame-gun.ps1 -OpenFrame` sets it for games whose sides were shown to
fill with game geometry.

Time Crisis at 16:9: 37 gameplay pictures with the sides 93.7-98.0% filled
(median 97.8%); the accuracy screen gives the same 9-point error (0.0052) as
at 4:3, and shots into the added sides register nothing. 2D sprites drawn in
the scene are not repeated in the sides.

## Motion cabinets (Namco System 22: Prop Cycle, Alpine Racer 1 and 2, Cyber Cycles, Aqua Jet, Air Combat 22)

The wire header appends a `motion` block after the picture: named axes the viewer
fills (steer, throttle, brake, stick x/y, throttle lever, lean, twist, pedal). The
gun fields keep their offsets, but the viewer and MAME are rebuilt together.
`MAME_VR_MOTION="PORT:axis[:flags];..."` maps axes onto the game's analog fields
(flag `h`: 0..1 drives the centre..maximum of a self-centring lever such as Prop
Cycle's pedal or Air Combat's throttle; `r`: reversed). A field named there is
never taken as a gun axis. Directions follow MAME's own input for each field.

`GunViewer --motion` (default for these games) uses thumbsticks and triggers:
left stick x steers, swings the skis and banks the plane; pulling the left stick back
climbs or leans back; the right stick's x edges the skis; the right trigger is
throttle and pedalling; the left trigger is the brake and the throttle lever.
Roll and tilt steering are off by default. `--body` is the opt-in body-motion
mode: the controllers' mean roll steers and edges, their heading against the head
swings the skis, the right controller's roll and pitch are the flight stick, and
moving the hands up and down in turn pedals. `motion_test.cpp` checks both
mappings with scripted controller poses. The shared presenter's theatre mode
now also reports the left grip pose, the left trigger, the right squeeze value
and both thumbsticks (`theatre_test.cpp` covers them).

The proof scripts (`axis <name> <value>`) drive each input from a fixed frame.
For each game, a no-input run repeated twice gives identical pictures, and every
input differs from the no-input run in 11 of 13 pictures. Directions seen in the
pictures: Cyber Cycles banks right and slows on brake or throttle-off; Air Combat
banks right, climbs with the stick pulled back (its own tutorial then says "push
the stick forward to descend") and lights the afterburner on full throttle; Prop
Cycle banks right and climbs with the handlebar pulled. Alpine Racer's swing/edge
and Aqua Jet's steer/lean directions are not yet confirmed by eye.
All six run at 16:9 open frame (gameplay side fill, median 75-97%).
