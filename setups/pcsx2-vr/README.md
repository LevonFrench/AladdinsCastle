# PCSX2 stereo experiment

The runner can capture a GS dump without a window, replay it with OpenGL, and
write a side-by-side PNG. The live Windows OpenGL frontend now renders both eyes
and has an in-process OpenXR stereo-screen presenter, native VR GunCon aiming,
and an experimental head-tracked projection mode.
The OpenXR submission path is compiled but still requires headset testing.

Virtua Cop and OutRun 2006 now have replay-validated pairs with near/far disparity,
exact stereo-off RGBA equality to unchanged replay, and exact HUD equality in a
diagnostic that suppresses displaced world draws. The depth-test guard pins
Virtua Cop's lock-on marker at zero disparity while retaining world depth.


## Current offline acceptance (supersedes older checkpoint counts)

All eleven profiles have been exercised; all nine additional titles reached
real gameplay. Eight of those nine have measured camera depth relations;
OutRun2 SP remains generic after three failed fitting approaches.

| Game | Screen depth/HUD/off | Turned INSIDE |
|---|---|---|
| Virtua Cop | Passed prototype scene | Offline scene aiming and projection pass |
| OutRun2006 | Passed prototype race | Replay pass; live full-speed gate failed3trials |
| Wipeout Fusion | Passed race | Offline zero/30 pass, effects limits |
| XGRA | Passed race | Stopped after3 launcher-proof failures |
| Time Crisis3 | Passed combat | Offline zero/30 pass, cover-blur defect |
| Crisis Zone | Depth/off/classified HUD pass | Zero/copies pass; complete HUD classification incomplete |
| Vampire Night | Passed combat with near-HUD policy | Offline zero/30 pass, flat flare effects |
| Ninja Assault | Measured combat; screen checks pass | Offline zero/30 pass after screen-copy correction; frustum holes |
| OutRun2 SP | Fit gate failed3approaches; generic retained | Unvalidated |
| Wipeout Pulse | Measured race; screen checks pass | Offline zero/30 pass, exhaust/blur artifacts |
| XGIII | Measured camera consensus; material artifacts remain | Offline zero/30 pass after screen-copy correction; flat light/trail effects |

These are frozen-scene and native copy/alpha/ordering proofs. No physical
headset composition, tracking, GunCon calibration, haptic feel or runtime
performance is accepted from offline tests. Camera/metre constants remain
provisional; original game FOV was not recovered by fitting Q against Z.

## Source and build

`upstream-pin.json` records PCSX2 commit
`904bd9da01c68bb31ae86f4ea6e0e9a1fb0743e2`, PenguinScreen2's reference commit, and
the preserved official Windows dependency archive's size and SHA-256. The source
recipe and pack headers agree on Qt 6.12.0, FFmpeg 9.0.1 and SDL3 3.4.18. There is
no per-tag Windows pack; the rolling release is overwritten. Preserve the pinned
archive. The old stable-FFmpeg header-precedence workaround is removed.

Apply `unattended-capture.patch` to that PCSX2 commit; it now includes capture,
OpenGL stereo, live eye-state duplication, the OpenXR screen presenter,
diagnostics and the SBS writer. Build the unchanged frontend and
runner first and preserve their binaries/resources separately. Build each
project's own `.vcxproj` with `/t:Build`, Release/x64, v145,
`/p:VCToolsVersion=14.51.36231`, `/p:PreferredToolArchitecture=x64` and
`/p:SolutionDir` set to the source root **with a trailing backslash**. Build the
patched frontend and runner the same way. Use a portable private data directory.
For this Windows OpenGL patch, copy the OpenXR include directory from the pinned
reference source to PCSX2 `3rdparty/openxr/include/openxr/` before compiling.
Keep the headers and existing loader private; preserve their included licenses.
There is no OpenXR link dependency: the loader is opened only when requested.

## Capture and replay

All executable, helper and file arguments must use native Windows backslashes,
including disc, BIOS, logfile and output arguments. Never open original disc
images: compressed-image indexes can be written beside them. Work only on private
copies. Launch hidden and below-normal, with null audio and an external process
time limit. The runner saves pictures internally; desktop capture is unnecessary.

Capture arguments:

- `-surfaceless -renderer gl -logfile <native-log-path>`
- `-capture-frame <N>`: presentation number, 1 through 36000.
- `-capture-output <native-png-path>`: PNG plus uncompressed GS dump.
- `-capture-bios <native-ROM0-copy-path>`
- Optional `-capture-pad-events <native-csv-path>`: sorted `frame,bind,pressure`
  rows. Bind names use DualShock2 names, pressure is 0 through 1. This changes
  emulated pad state on the CPU thread; it sends no operating-system input. Use
  explicit release events. Intended for local startup confirmations only.
- `-- <native-disc-copy-path>`

Capture forces fast boot, surfaceless graphics, null/muted audio, disabled
physical input sources, memory cards and network devices. A GS-thread counter
queues the dump and PNG, then drains snapshot work before stopping. The optional
pad event list receives progress through an atomic counter. Inspect the internal
log for BIOS/disc startup success and any early failure before retrying.

`PCSX2_CAPTURE_PAD` is the preferred controller script:

```powershell
$env:PCSX2_CAPTURE_PAD = '5:Cross;6:Circle;7:Start;8:DPadDown'
```

Each `seconds:button` entry produces a press and release on emulated port 1,
15 presentations apart on the capture's **60Hz timeline**: one quarter second.
Cross, Circle, Start, Up/Down/Left/Right and DPad aliases are supported; Triangle
also permits camera changes. Entries can be unsorted; the helper sorts events.
Each release must precede the chosen capture frame. Input is applied inside
PCSX2's pad state on the CPU thread, without OS mouse/keyboard events. Use either
this variable or the legacy CSV, not both. Other video cadences are unvalidated.

`PCSX2_CAPTURE_INTERVAL` optionally saves PNG/GS checkpoints at a presentation
interval within the same bounded run. Use these pictures to locate startup/menu
progress. Moving attract footage can be FMV: verify actual GS geometry before
claiming a 3D race. OutRun requires an offline progress profile and arcade
selection to reach a rendered race; its title/attract footage is a movie.

Replay needs neither a disc nor BIOS. Use the unchanged runner with
`-surfaceless -renderer gl -loop 1 -logfile <native-log-path>
-dumpdir <native-output-folder> -- <native-dump-path>`. **Explicit `-loop 1` is
required**: upstream configuration otherwise selects infinite replay. The same
arguments work on the patched runner. Compare decoded RGBA pixels with stereo
disabled, rather than PNG file bytes.

## Stereo controls and target isolation

The patched OpenGL vertex shader implements native-pixel displacement:

`dx = eye * 0.5 * separation_m * units_per_m * focal_px *
(Q * q_scale - 1 / (convergence_m * units_per_m))`.

For eligible triangles without camera Q, replace Q with `(Z-A)/B`. Shift only
varying-depth, depth-tested triangles. Sprites, constant depth and untested
overlays are pinned. A signed per-game Q scale must reconstruct positive inverse
distance; nonpositive mapped values are pinned. Conservative classification can
leave some world geometry flat.

Texture Q is not always camera Q. OutRun's environment-map passes demonstrate
this. A checked, target-specific affine model permits those passes and FST world
triangles to use Z instead. Where perspective Q agrees with the model within 64
Z units, use Q directly. Reject a loose aggregate fit; retain the consensus
selection, residuals and excluded passes as evidence. A target-width restriction
describes the observed camera/depth convention, not a universal game heuristic.

Environment variables are read once per process:

| Variable | Purpose |
|---|---|
| `PCSX2_STEREO` | 0/off or 1/on; default off |
| `PCSX2_STEREO_EYE` | +1 left, -1 right |
| `PCSX2_STEREO_SEPARATION_M` | Eye separation in metres |
| `PCSX2_STEREO_UNITS_PER_M` | Provisional game units per metre |
| `PCSX2_STEREO_CONVERGENCE_M` | Convergence distance in metres |
| `PCSX2_STEREO_FOCAL_PX` | Focal length in native GS pixels |
| `PCSX2_STEREO_Q_SCALE` | Q to inverse-game-distance scale |
| `PCSX2_STEREO_Z_A`, `PCSX2_STEREO_Z_B` | Checked affine Z/Q fit; B=0 disables fallback |
| `PCSX2_STEREO_Z_MODEL_WIDTH` | Restrict model to observed native target width; 0 accepts any |
| `PCSX2_STEREO_HUD_TARGET_WIDTH` | Explicit confirmed HUD target width to pin; 0 disables |
| `PCSX2_STEREO_TRACE` | Optional native CSV path for actual draw vertices |
| `PCSX2_STEREO_HIDE_DISPLACED` | Diagnostic only: suppress shifted world draws for HUD comparison |

Run the entire frozen dump twice in separate fresh processes, one per eye. Each
has its own GS device, cache, color/depth targets, temporary targets and feedback
history. Main and off-screen draws use the same displacement policy, with an
explicit exception for a demonstrated HUD render target. OutRun's separate
64-pixel C16/Z16 minimap is composited as screen UI and therefore remains flat.
Its main 640-pixel Z24 camera uses the separately checked affine model. This
provides complete paired target lifetimes for offline replay. Live runs use the
two GS states and presenter described below.
Both eyes begin from the same mono dump snapshot, so prior-frame effects require
additional investigation. Join matching eye frames inside the runner with
`-sbs <native-left-png> <native-right-png> <native-output-png>`.

Trace columns include Q, Z, primitive expansion, texturing, mode, depth-test
state, colours, texture dimensions, frame and native target width. Built-in
`-dump i,hw,tex` diagnostics expose raw GS state/vertices/textures. Use `-dumpdir`
to configure both hardware/software directories; a lone unconfigured directory
disables dumping. Bound diagnostic draw/frame ranges to avoid excessive data.
Fit `Z=A+B*Q` on eligible camera vertices, record residuals, and reject
a loose fit. An affine fit alone cannot determine real metres or camera FOV.
Physical calibration is a separate requirement.

## PenguinScreen2 study and credit

[PenguinScreen2](https://github.com/PenguinVRLab/PenguinScreen2), at the reference
pin, passes stereo parameters through the GS renderer and implements its active
geometric shift in Vulkan. Multiview chooses the eye sign. Q drives linear,
logarithmic or piecewise depth maps; uniform-Q perspective draws can be pinned to
the screen. Its optional Z/Q estimator derives inverse depth from varying,
depth-tested textured draws. Its clamped linear mode differs from this patch's
signed parallel-camera equation.

Its texture cache promotes display-chain targets to two layers, follows
render-to-texture feed edges, matches depth-layer counts and retargets source
views. Temporary depth and right-eye DATE stencil operations have known gaps.
OpenGL declares extra fields without applying the geometric shift at that pin;
Vulkan provides the multiview target support.

This implementation reuses the ideas of Q-driven displacement, conservative
screen-space classification and checked Z fallback, with credit to PenguinVRLab.
It instead uses OpenGL and independent whole-GS states/caches, avoiding selective
promotion of display-chain targets. Offline replay creates those states in
separate processes; live stereo selects two states on the same GS thread. Actual vertex logging and residual checks
support the fallback. No separately licensed bundled profiles are copied.
PCSX2 and this source patch use GPL-3.0-or-later terms.

## Validation and limits

Per-scene images, Q/Z fits, pixel comparisons and observations are kept privately.
Acceptance requires near objects to have larger disparity than far objects,
registered HUD geometry, and exact stereo-off RGBA equality to unchanged replay.
Transparent HUD pixels expose a different world background in each eye; compare
opaque interiors or the HUD geometry. The optional hide-displaced diagnostic
renders HUD and other pinned draws over identical backgrounds: both games have
zero changed pixels across all three resulting eye-frame comparisons.

| Scene | Measured correspondence and observations |
|---|---|
| Virtua Cop | Car 9px, far building 0px; lock-on ring reduced from 87-88px to 0px. Opaque hearts/ammo and diagnostic HUD agree. |
| OutRun 2006, in-car race | Hood 18px, near road 4px, far building 0px; full diagnostic HUD agrees. Camera cycle: chase, hood/dashboard in-car, bumper. |

The Virtua Cop marker is a coloured triangle-strip overlay using a white 128x128
paletted texture and ZTST ALWAYS. Its synthetic Q/Z does not supply a reliable
enemy-depth association, so the requested HUD fallback applies. The sampled
batch includes blue/amber tints; a fully red lock stage was not separately tested.
The guard is tint-independent. It has been replay-validated, including unchanged
baseline equality with stereo off.

Provisional scene tuning (not metric/FOV calibration):

| Parameter | Virtua Cop | OutRun 2006 |
|---|---:|---:|
| Separation, metres | 0.063 | 0.063 |
| Game units per metre | 10 | 1 |
| Convergence, metres | 10 | 1000 |
| Focal, native pixels | 320 | 320 |
| Q scale | 1 | -1 |
| Z A | -251.49612065 | -334.41641419 |
| Z B | 16777250.571979 | -1677707.992213 |
| Z model target width | unrestricted | 640 |
| Confirmed HUD target width | disabled | 64 |

Virtua Cop's fit maximum residual is below 1 Z unit. OutRun's mixed aggregate
fit is rejected; raw GS context separates minimap/depth conventions, and the
dominant main-camera consensus has R2 above 0.99999 and maximum residual below 32
Z units. The latter uses per-draw affine seeds, largest consensus and refitting,
with excluded projective-Q draws recorded. It does not reinterpret every Q as
camera depth.

Stereo work areas cover the true GS scissor for DATE/stencil/feedback/color-clip
operations, rather than clipping moved geometry to its old GPU bounding box.
CPU texture-cache validity and earlier source-copy bounds still warrant testing
for wider shifts and other scenes. Some constant-depth world details remain
flat; fine hood/specular and texture-edge aliasing differences remain visible.
No strong full-screen blur or speed-streak overlay appears in the selected race,
so those effects cannot receive broad acceptance from this proof. Static
reflection coordinates remain inherited from the game's commands.
Frozen screenshots cannot certify temporal motion blur, bloom or speed streaks
across a full game. Headset work still needs metric/FOV calibration, actual
quad appearance/orientation/recentering and effect testing. No VR services are
changed. Silent Scope scope analysis is skipped for this assignment.

No disc, BIOS, dump, picture, executable, log or operator pathname belongs here.
Run `tools/check_public_privacy.py` before each commit; do not push this experiment.

## Live Windows OpenGL screen

`PCSX2_STEREO_LIVE=1` together with `PCSX2_STEREO=1` duplicates the complete GS
submission stream on the GS thread. Each eye has its own GS local RAM, paths,
draw contexts, colour/depth texture cache, CLUTs, feedback targets and scanout
surfaces. Merge, weave/bob/blend and motion-adaptive histories, including the
motion-adaptive buffer phase, stay per eye. Scratch device surfaces used wholly
within one draw remain shared. Host presentation, ImGui, metrics and dump clocks
advance once. Host FIFO downloads return the left eye's result and advance both
eye states; live downloads are serialized on the GS thread. Eye shader signs
are left +1/right -1: nearby geometry moves toward the opposite eye's side.

Two completed GPU textures are retained at the full internal resolution. A
staging pair and publication mutex prevent mixed-frame eyes. GPU fences protect
handoff to a shared WGL context on the presenter's own thread.
The screen presenter adapts the ACVR implementation pinned in
`upstream-pin.json`; local controller and inside-camera extensions are described below.
`PCSX2_OPENXR=1` runtime-loads the existing `openxr_loader.dll` and creates two
fixed 1920x1536 OpenGL swapchains, once per session. Output-size changes never
recreate them: each eye is scaled to fit while preserving its pixel aspect,
and the quad's image rectangle covers only that used region. Each is submitted as a
quad visible only to its corresponding eye. The GL-to-quad copy flips rows;
actual runtime orientation remains a headset acceptance check.

The screen is placed in LOCAL space ahead of the initial head position/yaw.
Both stick clicks pressed together and released within 1 s recentre it (Home on the keyboard does too). Its width is distance times native image width divided by
`PCSX2_STEREO_FOCAL_PX`; height follows the display aspect. Thus screen size uses
the profile's game FOV, which remains provisional rather than calibrated.
`PCSX2_OPENXR_DISTANCE_M` defaults to 2.5 and `PCSX2_OPENXR_ASPECT` to 4/3.

Portable live runs use `-portable -nogui -fastboot <native-disc-copy-path>` and
`PCSX2_LIVE_HIDDEN=1`, with their own fresh configuration, BIOS and disc copies.
Use OpenGL, `upscale_multiplier=3`, null/muted audio and native XInput default
mapping for pad 1. Do not inherit an existing emulator configuration. Memory
cards may remain disabled for menu/attract experiments. Stop PS2.cmd writes
`PCSX2_LIVE_STOP_FILE` for graceful shutdown from the program's frame loop.
Launchers clear stale session files when no PCSX2 process is running and use a
Windows mutex for launch coordination. They require an already-running runtime; they must never start or
reconfigure SteamVR. All launcher/runtime files remain private.

With `PCSX2_OPENXR=0`, no loader or runtime call occurs. The presenter thread
still creates its shared GL context and can read the published textures. Proof
environment variables select GS-frame limits and internal pictures:

- `PCSX2_LIVE_STOP_FRAME`: presentation limit for graceful shutdown.
- `PCSX2_LIVE_DUMP_FRAME` and `PCSX2_LIVE_DUMP_PATH`: dump plus PNG; path must end
  in `.png` and use native Windows backslashes.
- `PCSX2_LIVE_SAVE_FRAME` and `PCSX2_LIVE_SAVE_PATH`: texture readback prefix.
  Writes left/right, an exact row-joined SBS, and shared-thread left/right PNGs.

The final live Virtua Cop attract scene retained 1536x1536 textures per eye at
3x. Main-context and presenter-context readbacks matched exactly in RGBA, and
the SBS was their exact row join. Nearby car correspondence was 19 pixels,
versus 4 for a distant building; opaque hearts and ammo matched exactly.
Both games also passed the live 3x hide-displaced HUD diagnostic with zero
changed RGBA pixels. OutRun's 1920x1344 live publication was checked from its
race dump, rather than new live race emulation. The final stereo-off runner
matched three unchanged baseline frames for each game exactly. A private
stop-file request shut down the live program gracefully.

Virtua Cop's sampled live emulation speed at 3x was 97.93-99.92 percent, slightly
below full speed. Picture readback/compression is a one-shot proof operation
and is omitted during ordinary play. Speed with the OpenXR presenter active
and live race-emulation speed remain unmeasured.

The final live scene and no-headset proof are recorded privately. Headset
submission, Home recentring, physical gamepad play and speed with OpenXR active
remain untested. The original two measured game profiles remain provisional. The later
measurement checkpoint below records two additional fits; the remaining seven
launchers still use unvalidated generic Q-only profiles. They do not constitute
whole-game stereo acceptance.

### Headset black-picture diagnostics

The owner's first headset run reached an OpenXR session but failed swapchain
creation with result -2. Output-size changes had repeatedly recreated the
chains, and the runtime reported GL error 0x0501 (`GL_INVALID_VALUE`). The fix
drains and logs GL errors before and after XR calls, including creation,
acquire/wait/release and frame submission. Both blit FBO statuses are logged;
only complete, successfully copied pairs are submitted. The first successful
`xrEndFrame` containing two layers and the final counters are explicit log lines.
If fixed-chain creation fails on the surfaceless context, the presenter destroys
that session and retries once on a shared context with a real, never-shown 1x1
Win32 window DC. It does not retry on every frame or output-size change.

The private `00 PS2 headset check.cmd` requires the owner's already-running
runtime, launches the first game, requests stop after 30 seconds, and records
swapchain success, submitted frames, drained GL errors and new runtime client
log lines. Graceful shutdown has a 15-second bound before terminating only its
own child. No-headset checks exercise the same fixed-size sRGB blit and verify
the used region against the vertically flipped published images. They do not
prove runtime swapchains or visible headset output. The corrected headset
picture remains an owner acceptance check.

### Right-hand GunCon 2 and controller model

The optional live gun path is gated by `PCSX2_VR_GUN=1`. A gun-game launcher
selects USB port 1 `guncon2`; racer launchers select `None`. Pad 1 retains
PCSX2's normal XInput DualShock 2 bindings. USB gun menu bindings additionally
accept the pad's Start, Back, D-pad and face buttons. Alternatives for a gun
button are combined before dispatch, so a released VR button cannot erase a
held pad button. Relative gun bindings must be absent: the absolute GunCon 2
path reads pointer 0.

The presenter creates right-hand aim/grip poses, analog trigger/squeeze, A/B
and haptic actions with Touch and Index suggestions. Only focused, tracked
poses publish input. Its aim ray intersects the exact placed screen quad;
coordinates outside that quad produce an off-screen shot. The emulation
thread uses the inverse GS display rectangle to update InputManager pointer 0
and normal input events, including hidden 640x480 surfaces at 3x. No operating
system mouse movement is used. A 500ms publication lease and tracking/focus
loss release the gun. Built-in GunCon per-game scanline/calibration parameters
remain in use. Normalized calibration is centered:
`u'=(u-.5)*scale_x+.5+offset_x`, likewise for y.

Gun profile settings provide offset/scale, aim-dot/model toggles and grip
mapping. `PCSX2_VR_GUN_OFFSET_X/Y` default to 0 and `SCALE_X/Y` to 1.
`PCSX2_VR_GUN_DOT` and `PCSX2_VR_GUN_SHOW_MODEL` default to 1.
Either grip maps to C in every gun profile (the contract's Time Crisis cover rule, M5). Start and Select have separate inputs;
A no longer doubles as Start. The screen-depth dot is drawn
at the same quad UV in each eye after the game-picture copy.

The renderer reads the owner's existing `con-pistol-dpad.glb` and matching
TOML through `PCSX2_VR_GUN_MODEL` and `PCSX2_VR_GUN_TOML`. It locks the GLB grip
anchor position to the tracked hand, aligns the real muzzle bore to the tracked
aim direction, derives roll from grip-up projected around that direction, and renders a
transparent per-eye projection layer over the game screen. The model uses
metres, +Y up and -Z forward. Trigger input rotates both named trigger pivots
about the TOML axis over its specified range; the supplied metadata uses
`pivot_trigger`, `pivot_trigger_lod1`, axis [-1,0,0], and 0..0.35 radians.
A delivered USB trigger edge requests a 35ms strong pulse and a visual-only
2cm backward/1cm upward kick decaying over 100ms. Recoil never enters aim math.
The original model and its CC0 material stay in the existing project asset
library; no proprietary game geometry is used by the controller model.

With OpenXR disabled, `PCSX2_VR_GUN_SCRIPT` accepts
`seconds:u:v:trigger;seconds:u:v:trigger` on the diagnostic 60Hz presentation
clock. Each event holds aim and trigger for 15 ticks. It passes through the
same ray intersection, mailbox, InputManager binding, GunCon position and USB
packet paths. Nonfinite/invalid entries and scripts with OpenXR enabled are
rejected. `PCSX2_CAPTURE_PAD` supplies native menu button pulses.
`PCSX2_CAPTURE_INTERVAL` plus `PCSX2_LIVE_DUMP_PATH` saves internal live
checkpoints; all file arguments and environment paths use native Windows
backslashes. `PCSX2_VR_GUN_MODEL_PROOF` selects a private prefix for the same
renderer to save trigger-0/1 GPU pictures without loading OpenXR.

Hardware tracking, actual Touch/Index bindings, visible controller placement,
haptic feel, and all five games' in-headset calibration still require the
owner's headset. A game-generated calibration cross is distinct from a combat
hit or a live-headset acceptance result; private evidence records each gate.

No-headset validation for this input/model change: Virtua Cop's own calibration
crosses matched three normalized points within 0..3 upscaled pixels at 3x,
without the presenter dot. A live File 1 frame showed the game's blue shot
effect at the requested center and a spent round. Shared-thread eye readbacks
and the exact SBS row join passed on that frame. GPU trigger-0/1 renders
changed only the trigger region. Four other gun profiles passed bounded
startup/USB automatic-configuration checks; their firing/calibration remain
unaccepted. No connected physical XInput pad was available at final checking;
scripted XInput events proved hidden menu dispatch. No OpenXR runtime was
started, so physical tracking, haptics, model fit, dot and sustained headset
performance remain owner checks.


### Common VR layout and corrected gun orientation

`PCSX2_VR_CONTROLLERS=1` enables the common menu controls in every profile.
Actions require focus; menu inputs do not require a tracked aiming pose.
Inputs use Pointer-0 buttons 12-15 (Select, Start, L3, R3) through InputManager, with in-memory
Pad1 alternatives and USB1 alternatives for gun profiles. Source aggregation
preserves held Xbox buttons when a VR source releases. Pad1 INI bytes remain
unchanged. The action set has Touch and Index suggestions:

| VR control | PS2 input |
|---|---|
| Left stick click, alone (U1) | Start |
| Right stick click, alone (U2) | Select |
| Both stick clicks together, released within 1 s (U3) | Recentre the screen or inside view at the current head position and yaw |
| Both stick clicks held 3 s (U4) | Quit and save: graceful VM shutdown, memory cards flush, back to the Hub. A ring in each eye image shows the hold: nothing for 1 s, filling to 3 s, one flash, then the quit (gone at once if a click lets go) |
| Left Y (Index left B) | L3 (native pad); in a pad game (below) Triangle |
| Left X (Index left A) | R3 (native pad); in a pad game (below) Square |
| Gun profile: right trigger | GunCon trigger; off-screen aim reloads |
| Gun profile: right A / B | GunCon A / B |
| Gun profile: either grip | GunCon C |

### Pad games: Touch as the whole DualShock 2

A game with no gun (the racers: Wipeout Pulse and Fusion, OutRun 2 and 2006, XGRA, XGIII) has USB1 `None`.
Before 2026-10-26 the Touch controllers gave such a game only Start and Select, so the owner could press
"Press Start" in Wipeout Pulse but not confirm anything after it (headset report 2026-10-22, "no way to progress").
With `PCSX2_VR_PAD=1` (set by the launcher for every non-gun game) the presenter also sends:

| Touch | DualShock 2 |
|---|---|
| Right A / B | Cross / Circle |
| Left X / Y (Index left A / B) | Square / Triangle |
| Right trigger / left trigger | R2 / L2 |
| Right grip / left grip | R1 / L1 |
| Left stick | Left analog stick, and the D-pad past half travel |
| Right stick | Right analog stick |

Start, Select, recentre and quit stay on the stick clicks (U1-U4). L3 and R3 are off in a pad game because
their buttons are Triangle and Square. `PCSX2_VR_PAD_RT` and `PCSX2_VR_PAD_LT` (`cross`, `square`, ...) move a trigger
onto another button for a game that wants it, as the launcher does for the racers.

Offline proof input (OpenXR must be off): `PCSX2_VR_PAD_SCRIPT="seconds:button[:hold];..."` presses buttons on the
60 Hz frame clock (`start`, `select`, `cross`, `circle`, `square`, `triangle`, `l1`, `l2`, `r1`, `r2`, `up`, `down`, `left`,
`right`, `lup`, `lright`, `ldown`, `lleft`, `rup`, `rright`, `rdown`, `rleft`). `PCSX2_LIVE_SAVE_LIST="f1,f2,..."`
(with `PCSX2_LIVE_SAVE_PATH`) saves the left eye at each listed frame, for a film strip of a scripted run.

This follows the universal controls contract (2026-10-25). No menu or system
button is bound on either controller: under ALVR the left menu button opens the
SteamVR dashboard, and the right system button belongs to the headset. The Touch
left menu "spare Start" alias is gone; so is the Index left B spare Start.

Chord resolver (`pcsx2/Input/PS2VRStickChord.h`, in the patch): a stick click
waits up to 150 ms for the other one. If it comes, the pair is a chord and no
Start or Select is sent at all. Otherwise the click is sent as pressed for as long
as it stays held. A tap shorter than the wait is stretched to a 100 ms pulse so the
PS2 pad still sees it. A chord released within 1 s recentres, and a chord held 3 s
quits; the recentre is then not sent. Letting go of one click cancels the quit
countdown. The stick clicks no longer reach the pad's L3 and R3, so a game that
needs them gets them on left Y (L3) and left X (R3). `PCSX2_VR_L3R3=0` switches
those two off. The pad's normal XInput L3/R3 bindings are unchanged. The change is in
`unattended-capture.patch` and has not been run in a headset.
`stick_chord_test.py` builds `stick_chord_test.cpp` against the header taken from
the patch, checks the timings on the CPU, and pins the bindings above.
`PCSX2_VR_MENU_SCRIPT=seconds:Select;seconds:Start;seconds:L3;seconds:R3`
provides bounded diagnostic pulses only with OpenXR disabled (they bypass the
chord resolver). Focus loss or a 500ms stale publication releases every VR control
and resets the resolver. Gun tracking loss releases gun input while preserving
focused menu inputs.

The gun model uses the muzzle anchor's actual -Z bore rather than assuming
the GLB grip axes match OpenXR grip axes. Its grip position stays in the hand;
trigger animation and recoil do not alter aim. The GPU proof writes side/rear
trigger-0/1 views with cyan aim and orange muzzle rays. Rear occlusion can hide
the trigger; side views and both pivot matrices establish its motion. Actual
controller fit and haptic feel still require physical testing.

### Experimental inside-game projection

`PCSX2_INSIDE=1` selects GS vertex reconstruction and an OpenXR projection
layer instead of the game-screen quad layers. Existing screen launchers keep
their screen behavior. Separate INSIDE launchers share the same controls.

For positively classified perspective draws, signed Q (or the target-specific
Z/Q fit) supplies inverse game-camera distance. The vertex shader recovers
view-space x/y/z using the canvas centre, focal lengths and game-units scale,
then applies the recentered eye inverse pose and asymmetric OpenXR projection.
Original screen-affine GS attributes are transported as attribute*original-W
plus a smooth original-W weight; their fragment ratio preserves texture ST/Q,
FST, smooth colour, fog and AA interpolation under the triangle homography.
True new clip W permits homogeneous clipping. Depth uses infinite reversed-Z.

The presenter publishes predicted eye poses/FOVs. The GS latches one immutable
packet between completed pairs and publishes its exact packet with the GPU
images. Projection views use those rendered poses, rather than newer poses.
Home resets the origin position/yaw and the stationary HUD panel. Small
nonperspective sprites, text and constant-depth primitives use that plane;
the aim dot is projected at the plane separately for each eye. The optional
tracked gun remains a transparent projection layer.

Settings: `PCSX2_INSIDE_CANVAS_WIDTH/HEIGHT`, `TARGET_WIDTH`, `FY` and
`HUD_DISTANCE_M` (private launchers select 2.5m). Existing focal, Q scale,
Z coefficients and units/metre apply. An explicit `FAKE_YAW_DEG` diagnostic
uses the original game FOV without a runtime; +30 turns left. At exactly zero
the original stereo position branch is retained for bit-exact regression.
This diagnostic does not assert that the physical headset FOV matches the game.

Pass classification is provisional. Main camera targets are selected by
native width; intermediate targets keep the original GS projection to avoid
double-transforming textures later mapped into the reconstructed scene.
Full-canvas clears and textured effects retain raster placement for the same
reason. Large HUD, mirrors, blur/bloom, feedback, software depth tests and
CPU readbacks can still break. Existing GS scissor/dirty/copy bounds and
game-side culling limit retained geometry. No renderer can recover geometry
the game never submitted; disocclusion and original-view holes remain.

Virtua Cop and OutRun use measured depth fits but provisional focal/metric
constants. Generic profiles remain unvalidated. Native source/math tests,
internal GPU images and physical headset acceptance are distinct gates;
the latter is not established by an OpenXR-disabled proof. No runtime is
started or configured by diagnostic runs.


### Additional gameplay depth measurements

The capture runner accepts `-capture-save-state <private-state>` and
`-capture-load-state <private-state>` only together with capture mode. These
opt-in VM checkpoints shorten scripted menu navigation; they are separate
from memory-card saves. Keep cards disabled. A checkpoint follows completion
of the requested picture/dump plus at least twelve further presentations, so
it is a resume aid rather than the exact pictured state. Save/load errors
return failure; normal dump replay retains the previous behavior.

Scripted native pad input reached actual arcade races for the following games.
Both use the observed 512-wide camera target and 512x448 display canvas.
Numbers below fit `Z = A + B*Q`; residuals are in GS depth units, not metres.

| Game | A | B | Fit RMS | Offline validation |
| --- | ---: | ---: | ---: | --- |
| Wipeout Fusion | -1043.5043267045985 | 8350519.785445882 | 1.7562 | Near ship19px versus distant sign2px; flat HUD; zero-pose exact; fake-left30 rendered |
| XGRA | -32.49576158036507 | 1073741.6504301124 | 0.2958 | Near bike24px versus distant sign1px; flat HUD; INSIDE proof incomplete |

XGRA's separate high-speed race sample supports the same relation: held-out
RMS0.2943 with99.52% inliers. Unchanged-runner versus stereo-off comparisons
are exact in all three saved frames for both games. Initial GS-state scanout
buffers can remain mono until a complete redraw; use the completed final
frame for stereo acceptance. Isolated pinned draws include the visible HUD
and match exactly between eyes. Wipeout's exhaust glow and sparks can remain
flat and separate from the displaced ship; XGRA's fast rider trails remain
a temporal-effects acceptance limit.

Wipeout's launcher-engine dump replays passed screen/zero equality (including
alpha), both shared-context readbacks, exact SBS joining and a30-degree fake
head turn. XGRA's first launcher proof saved a blank startup buffer. A later
screen proof succeeded, but its zero-pose run exceeded the180-second bound;
a shorter-warm-up retry also exceeded that bound, leaving one shared-readback
PNG unfinished. Native testing stopped after these three unsuccessful proof
attempts. The root cause is unresolved; no GL error was logged. This is not
headset or live race speed acceptance.

Time Crisis3, Crisis Zone, Vampire Night, Ninja Assault, OutRun2, Wipeout Pulse
and XGIII have not been measured in this checkpoint and retain generic profiles.
All original and INSIDE launcher wrappers remain in place; only the two
measured private profiles and common offline diagnostic arguments changed.
Focal length/centre, convergence and game-units-per-metre remain provisional
visual choices. The Z/Q fit does not recover physical scale or lens calibration.
No game data, private configuration, dumps, pictures, VM checkpoints, logs or
built programs are included here. No OpenXR runtime was started and nothing was pushed.


### Controller overlays above the game image

The frame composer constructs a single ordered list for both presentation
modes. Screen mode submits left and right game quads, then a controller
projection. INSIDE submits the reconstructed scene (including its fixed HUD
panel), then the controller projection. No game/HUD layer follows that overlay;
no composition-depth extension is attached to it. Its flags include
`XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT` and straight-alpha input.

The overlay is cleared to transparent black every frame. The gun draws first,
then the aim dot draws with depth testing disabled into the same overlay. The
dot therefore stays above the screen and HUD, including when the gun model
toggle is off; neither visual is baked into the opaque game swapchain.
The INSIDE dot uses the retained scene intersection (or far fallback), through
the same projection as controller input. Classified HUD draws use near reversed
depth and an unconditional depth test so the world cannot hide that panel.
The model/dot projection remains after the game/HUD image.

`PCSX2_VR_LAYER_PROOF=<private-prefix>` with OpenXR disabled exercises that same
overlay renderer on the live shared GL context. It saves per-eye gun RGBA and
alpha PNGs, dot-only PNGs after clearing the model, and both composition plans
as JSON. Plans explicitly say `submitted_to_runtime: false`. With OpenXR on,
the first successfully submitted controller frame logs actual back-to-front
layer names/types/flags, and optionally saves the corresponding plan. Private
hidden/muted/below-normal timed runs passed in screen and INSIDE modes: opaque
gun silhouettes, alpha/RGB zero outside the model, only25 opaque dot pixels on
the subsequent frame, and no GL errors. These checks do not prove physical
SteamVR composition; the owner must retest that in the headset.


### INSIDE controller ray and original game camera

INSIDE input does not use the screen quad intersection. Main-camera perspective
triangles are retained once per completed pair, using the same Q/Z classifier,
inverse-distance scale and focal lengths as the vertex shader. The right aim
ray is transformed into the recentered game camera, intersected with the
nearest retained triangle, then projected into original-picture UV for GunCon2
pointer0. A miss uses a100m point along the ray. UV outside the original picture
remains off screen for reload. Geometry snapshots expire after500ms and have a
32768-triangle cap; only gun profiles collect them. Pass classification, hidden
surfaces, source-game culling and provisional metric scale remain limits.

The original scanout is part of this camera conversion. The GS target can be
cropped, offset or stretched when forming the game picture. Reconstruction now
maps target pixels through that source/display rectangle, and applies its
inverse after eye projection. The CPU scene triangles use the same mapping.
The tested Virtua Cop scene reads512x256 and displays512x512, so its vertical
scale is2. Assuming an identity mapping displaced HUD markers and shots.

Hidden native tests with fake head-left20 reached actual gameplay, acknowledged
an accepted scene-hit shot and off-screen reload, and saved the game's own
shot graphic/spent round. Two visible game calibration crosses matched their
independently projected positions within2upscaled pixels at3x. The third sampled
point lies beyond that turned view. Exact-zero INSIDE and screen dump replays
match in every RGBA channel for both eyes; shared-context copies and SBS joins
are exact, with no GL errors. These are offline checks, not headset acceptance.

`PCSX2_VR_GUN_SCRIPT` retains original-picture target points for calibration;
INSIDE scripts traverse the same triangle query and camera projection as live
input. `PCSX2_VR_GUN_SCRIPT_SPACE=head` instead supplies head-relative angular
points from a deterministic turned-hand pose. Both require OpenXR disabled.
Native tests cover translated origins, nearest hit, fallback, invalid/backward
rays, head-relative20-degree projection, and input pulse release. Physical
tracking, game-specific calibration beyond the sampled title, muzzle fit and
haptics still require the owner's headset check.


The overlay proof also exercises the scene-distance dot consumer with a0.8m
fixture and the actual fake-head20 eye matrices. Independently projected UV
matches the native dot centre to less than1pixel; left/right dots are53pixels
apart in1024px eye images. The trigger/recoil fixture moves the gun while the
dot pixels remain identical. Separate native side/rear trigger0/1 renders check
the supplied model, both trigger-node transforms and muzzle/aim alignment;
the trigger is occluded from the rear. Alpha/RGB is zero outside the gun/dot.
This is a deterministic visual fixture, separate from the real game shot proof.
Live recoil/haptic dispatch remains keyed to the GunCon USB accepted-shot serial,
with35ms amplitude1 feedback; actual vibration and tracked fit require a headset.


### Reliable controller quit and private Stop helper

Both stick clicks held for 3 seconds request graceful VM shutdown (U4) through
the ordinary CPU-thread stop route, in every game; memory cards flush as the VM
stops and no save state is made. It remains available when gun aim tracking is
absent. Release/focus loss resets the monotonic hold; a held chord fires once.
Left Y is no longer the quit button. Xbox mapping is unchanged. The offline-only
injected hold feeds both clicks to the same resolver and gracefully exits the
actual native program even with a frame limit.

The private Stop launcher writes the stop file first, waits15seconds, then forces
only the exact private executable and a creation-time-verified launcher helper.
It holds process handles and rechecks creation time on the terminate handle.
Missing/wrong helper timestamps are reported and left running. Startup, Stop
selection and lease changes share a short gate; waits happen outside it. A
still-live stamped launcher retains its shared logs until receipt capture ends.
Hidden native checks passed graceful Stop and hold-to-quit, stubborn fallback,
foreign same-basename exclusion, wrong/missing stamps, and deterministic
startup/replacement interleavings. Independent source review and two repair
rounds found no remaining ownership issue within scope. Physical action delivery
still needs an owner test. Play audio remains on; offline checks remain muted.


### Recommended INSIDE eye grid and bounded upscale policy

INSIDE queries both primary-stereo view recommendations with
`xrEnumerateViewConfigurationViews`, validates the sizes against runtime and GL
limits, and creates each eye chain once at that size for the session. The gun
projection uses those eye sizes too. Each complete reconstructed GS eye is
blit-scaled into the entire recommended rectangle. This final grid is distinct
from GS geometry sampling: GS retains3x/4x render/depth targets, so a larger
recommended grid still scales those samples. Screen mode keeps its existing
fixed1920x1536 chains and used rectangles.

The smallest permitted GS scale that covers the recommendation is selected,
capped at4x and by `PCSX2_INSIDE_MAX_UPSCALE`. CPU-thread application uses the
existing MTGS settings route without persisting an owner setting. After30seconds,
five consecutive speed samples below99.5% at least2seconds apart request3x;
it never oscillates back to4x. A fresh game-setting reload can reapply the
selected scale while the chains remain fixed. These timings/thresholds passed
native tests. The offline-only synthetic-eye switch never loads OpenXR and is
explicitly labelled as injected evidence.

Actual hidden gameplay at fake head-left20 sampled100% in Virtua Cop at4x,
with2048x2048 GS eyes. OutRun2006 did not meet full-speed acceptance: a4x-to3x
trial timed out at240seconds; fresh3x race samples were13.98–22.42%, and a3x
zero-pose control without periodic screenshots was12.30–21.76%. The zero-pose
control also being slow prevents assigning the cause solely to head projection
or screenshot pauses. No GL error was logged. Three full-speed gate attempts
are retained; further OutRun speed tests stop at this checkpoint.

The successful runs saved both shared GPU eyes and exact RGBA SBS joins.
A synthetic2592x2688 final grid passed the same native blit/FBO path and an
independent nearest-sample comparison, allowing only adjacent texels at exact
floating-point texel-boundary ties. All other pixels match exactly. Real
runtime query/allocation, tracked overlay visibility and headset frame rate
still require owner testing. No headset runtime was started.


### Time Crisis3 measured combat checkpoint

Actual one-player Arcade Stage1 supplies three training GS dumps plus an
independent fourth. On640-wide camera targets the fit is
`Z=-2559.855954779021+1677955.5163746427*Q`, Qscale+1. Held-out106090 rows have
RMS0.2891932036 and maximum residual0.5239128587, all within guard64. Source
scanout640x256 displays640x512, vertical scale2. Lens and metric scale remain
provisional: fx320,fy341.3333,canvas640x512,units/m1,convergence1000m.

Near enemy image disparity is17px versus2px for distant palms. All replay
stereo-off RGBA pixels equal the unchanged runner; HUD-only eyes are identical.
Actual private launcher dump proofs pass fake-left30, exact-zero vs screen,
shared GPU eyes and native SBS, at3x/1920x1536. This is depth/replay acceptance,
not physical GunCon calibration or live/headset speed acceptance. Blurred cover
remains a wrong flat layer in INSIDE, small sparks stay pinned, and culling/sky
pass edges are visible. Six other generic profiles remain unmeasured.

### Crisis Zone measured combat checkpoint

Story-mode Garland Square combat provides three training dumps and an independent
fourth. Main512-wide-camera depth is
`Z=-40.4565262660617+3355399.411392285*Q`, Qscale+1. Training77497 rows have
RMS0.294305203; held-out26819 rows have RMS0.295139967 and maximum3.543243138,
all within guard64. Four alpha-textured32x32 vertices have larger residuals;
they remain included, with no assigned cause. Scanout512x512 is unscaled.
Lens/metric settings remain provisional: fx320,fy426.6667,canvas512x512,
units/m1,convergence1000m.

Near enemy18px versus distant books5px. Stereo-off equals the unchanged runner;
classified-HUD-only eyes match exactly. Native launcher fake30/shared/SBS and
exact-zero RGBA checks pass at3x1536x1536. Complete HUD classification is not
accepted: some score/ammo/frame pieces occupy inconsistent INSIDE planes;
muzzle-flash planes tilt across geometry and culling/disocclusion gaps remain.
Whole-title effects, physical calibration and live/headset speed are untested.
The game's own controller page shows Release reload mode: hold Reload/action
to attack, release to cover/reload. Five generic depth profiles remain.

### Vampire Night measured combat and near HUD

Village combat gives three training dumps plus an independent fourth. On640-wide
camera targets, `Z=-3.0600161980837584+167772.14645252345*Q`, Qscale+1.
Training87169 rows have RMS0.3321038441; held-out22012 have RMS0.3349568146,
maximum1.344388458. Native scanout640x224 displays640x448, rasterY2. Provisional
units/m10,convergence1000m,fx320,fy298.6667,canvas640x448; lens/metres unmeasured.

The perspective life/ammo HUD follows the same Q/Z relation but sits in a
separate extreme-near range45.19..51.73, versus world Q up to1.0135. An opt-in
`PCSX2_STEREO_HUD_NEAR_Q=10` pins draws only when every vertex qualifies on the
fitted target, using camera Q or Z fallback. Default0 disables this policy.
Native before/after pictures show the life models restored to the flat HUD;
all3 isolated HUD eyes match, and all3 stereo-off frames equal unchanged replay.
Near armour19px versus distant building2px. Actual launcher fake30/shared/SBS
and exact-zero RGBA pass at3x1920x1344. Hit flares/radial streaks remain flat and
may ghost; culling holes persist. Physical calibration/headset/live FPS remain
unverified. The latest executable also passes screen/INSIDE final gun/dot layer
order, transparent-black background and stale-pixel checks without OpenXR.
Four generic profiles remain: Ninja Assault, OutRun2, Wipeout Pulse and XGIII.

### Ninja Assault measured depth; INSIDE visual defect

Guren Chapter actual combat fits `Z=-7.219639211177309+16777004.43017631*Q`
on512-wide targets,Qscale+1. Train25517/25639 inliers,RMS0.2801723065.
Held-out9150/9310 within64,inlierRMS0.2756298509,max0.5019095387; ALL-row
RMS21.14779286,max191.77111686. The160 outlier vertices use128x128 texture
and disable depth writes; a native traced replay confirms fitted-Z mode2 for
all160. No unique material/effect identity is assigned. Scanout512x224 displays
512x448,rasterY2. Provisional units/m1000,fx320,fy373.3333,convergence1000m.

Screen stereo near armour36px/far stone7px, isolated full HUD eye equality,
and unchanged stereo-off RGBA checks pass. Private launcher shared/SBS and
exact-zero RGBA checks pass at3x1536x1344. The30-degree INSIDE picture is
VISUALLY UNACCEPTED: duplicate enemy/HUD image and dark regions remain,
beyond ordinary missing-view holes. A feedback/history/pass interaction is
only a hypothesis. No physical calibration or live/headset speed claim.
Red sword trails/whole-title effects remain unvalidated. Three generic
profiles remain: OutRun2,Wipeout Pulse,XGIII. Normal screen mode is the
accepted stereo result for this scene; INSIDE retains its experimental status.

### OutRun2 SP fit stop

The slot labelled OutRun2 identifies as Special Tours. Actual Arcade race,
F50/AT,progressive scan,163km/h reached with cards disabled. Three fitting
approaches failed the existing camera eligibility check: all640-wide negative-Q
candidates,depth-writing subset,tighter1-unit fitting consensus. Selected inlier
fractions remain about65%,with guard64 RMS15.9..16.7. These are inlier metrics,
not universal residuals. The tight64-wide group does not identify the main
camera and is not substituted. No fourth fit or Z model/profile promotion;
generic state and stereo/INSIDE proof remain unaccepted. Only third-person
chase view observed; first-person/cockpit and stereo effects unverified.

### Wipeout Pulse measured race checkpoint

Actual Racebox Talons Junction race supplies three training dumps plus held-out
fourth. Main640-wide negative-Q fit is
`Z=8.530184893672867-182357.79064065573*Q`,Qscale-1. Training
161563/175029 within64,inlierRMS7.41891. Held-out44866/51774 within64
(86.6574%),inlierRMS10.23815; ALL-row RMS49.75422,max328.47261.
This is a dominant approximation, not a universal affine relation. Native
traced replay confirms all6908 outside-guard world vertices use fitted-Z.
Provisional units/m10,fx320,fy298.6667,canvas640x448,convergence1000m.

Near ship90px/far gate1px; one completed replay PNG per eye has exact
stereo-off/baseline RGBA and isolated HUD equality. Native launcher fake30,
shared/SBS and exact-zero pass at3x1920x1344. Exhaust/blur cutouts or
stippling and flat sparks remain; culling holes expected. Close/Far chase
views confirmed in own options, first-person/cockpit unconfirmed. Physical
headset,live FPS and whole-title effects untested. OutRun2 remains generic
after its fit stop; XGIII is the last unattempted profile.

### XGIII measured camera; INSIDE visual defect

Actual Palus/Lithium/Muon Harbor race reached. All-candidate fit was ineligible;
depth-writing training subset gives corroborated main640 camera consensus:
`Z=-43008.36301120091+687194966.8812681*Q`,Qscale+1. Train182673/288882
within64,inlierRMS1.685744. Held-out including nonwriting passes:57050/96768
within64(58.9554%),inlierRMS1.610554,max62.5823; ALL-row RMS7405726.596,
max300380984.762. This is not universal texture Q. Native39718 outside-guard
world vertices all use fitted-Z. Provisional units/m10,fx320,fy341.3333,
virtualcanvas640x512,convergence1000m; output640x448,source640x256.

Near barrier19px/tail31px versus far bridge3px, with imperfect near registration
and remaining material artifacts. All3 stereo-off/unchanged and isolated-HUD
RGBA comparisons pass. Native launcher shared/SBS/exact-zero pass at3x1920x1344.
Fake30 INSIDE is VISUALLY UNACCEPTED: duplicate HUD/scenery,detached translucent
light/trail planes,dark cutouts beyond frustum holes. No unique cause assigned.
Select confirms bike-free internal/first-person view, without visible cockpit;
proofs use chase, not every camera preset. Physical/live FPS untested.

### INSIDE scanout-aware screen-copy follow-up

The initial Ninja Assault/XGIII duplicate-image defects above are repaired in
the inspected frozen scenes. `PCSX2_INSIDE_FEEDBACK_PASSTHROUGH=1` is enabled
for these two profiles only; default0 preserves earlier policy. Main-camera
nonworld flat-depth draws sampling a render target stay in picture space if
scanout-mapped bounds cover97.5%of the canvas. Sprite depth uses each pair's
bottom/right endpoint, matching GS expansion; ignored wrapped endpoint Z is
not a second camera depth. Classified varying-depth world batches retain
head reprojection even when their bounds cover the viewport.

Native same-build,matched-GS-tick31 legacy/corrected comparisons show Ninja
Assault108 screen-copy records wrongly classified as HUD become0;all144
identified copies pass through. XGIII72wrong HUD copies become0;232large
world draws now stay under reprojection. Both eyes inspected:duplicate
enemy/scenery/HUD removed. Original-frustum holes remain;XGIII translucent
light/trail planes still detach. This is frozen-scene acceptance only.

Both profiles have exact-zero RGBA versus matched screen,exact shared GPU
eyes/native SBS,and no GL errors. An initial XGIII comparison used different
recorded ticks/timer values and is rejected;matched tick31 passes. Frontend
and own-project runner v145 builds pass;new runner stereo-off equals the
unchanged pinned baseline in3frames. Gun/dot final-order and transparent
background proofs also pass the new executable in both launcher modes.
`PCSX2_INSIDE_PASS_TRACE` optionally records the classifier/raster decisions;
normal play leaves tracing off. Physical runtime/performance remains untested.

### Backend lifecycle corrections

Private play launchers have no session time cap. Offline runs require an explicit
time limit or bounded check/replay mode. A requested stop allows 15 seconds
for graceful shutdown before the verified owned-process fallback. CPU policy
fixtures cover play beyond two hours, timed exit and an externally requested stop.

Play enables one private 8 MB file card per game. Offline and diagnostic runs
disable every slot and select a separate unused card directory. Xbox bindings
are preserved. Normal VM shutdown closes the card and flushes its C file;
the save-state boolean does not disable memory-card persistence. Forced
termination may still lose an unfinished write; in-game save/load needs owner testing.

The OpenXR presenter acquires/waits outside the publication mutex, using
10 ms waits within a 250 ms budget. A timeout skips the frame and retains
the acquired image for retry. Game and gun chains use the same policy.
Blits and the exact published poses are copied together under the lock;
a consumer GL fence prevents source reuse, with CPU completion outside
the lock. No extra swapchain wait is made during destruction.
A native offline delay hook and a concurrent CPU fixture exercise timeout,
publication progress, recovery and interruptible shutdown without OpenXR.

A lone stick click released before the 150 ms chord wait ends is sent as a
100 ms pulse. Holding both clicks for the 3 s quit never sends Start or Select.
`stick_chord_test.py` covers the short tap, long hold, chord boundary, quit, cancel and focus reset.

Depth review only: nine profiles retain inherited 1000 m screen convergence;
Virtua Cop and the generic OutRun 2 profile use 10 m. These are provisional,
not measured physical distances. INSIDE uses runtime eye poses and bypasses
the screen convergence shift; its HUD distance is explicitly 2.5 m. No profile
constants changed. A future approved screen trial should compare convergence
with the physical quad distance, retaining the current reference. Screen-mode
separation could come from the runtime eye baseline; fake-pose proofs retain
a fixed baseline. Physical scale, comfort and actual save/load need owner checks.

Backend offline verification: the v145 live build boots with OpenXR disabled.
During an injected swapchain wait, real emulation advances published pairs1
to6, then the same wait policy resumes; the timed stop exits gracefully.
Matched dump replays have exact shared-texture/SBS copies, and both the
injected-wait picture and INSIDE zero-pose picture equal the screen reference
pixel-for-pixel. These tests do not establish headset timeout recovery,
controller feel, long-session stability or in-game save/load acceptance.


### OpenXR reproducibility pins

The consumed headers are OpenXR 1.1.61, from PenguinVRLab/PenguinScreen2 commit
`f1eb4c813af0eba2ea6cedcd3dbb8d125b589c07`, under `3rdparty/openxr/include/openxr`.
`upstream-pin.json` records a pinned source URL and SHA-256 for every header,
including both repository-blob and installed bytes where checkout newlines differ.
This is distinct from the preserved loader DLL, whose version resource is 1.1.43.0
and whose existing SHA-256 remains pinned. No runtime is opened to identify it.

The pin now contains a source-reviewed replacement loader recipe for the same
pinned vendored SDK: existing x64 MSVC/CMake/Ninja/Python, `BUILD_LOADER=ON`,
`DYNAMIC_LOADER=ON`, tests/layers/samples off, vendored jsoncpp and no forced
regeneration. Build only the `openxr_loader` target. Nothing was built, fetched
or installed for this record; the recipe is unexecuted and must not replace
the current DLL automatically. Its SDK version is 1.1.61, so it is not a claim
that the preserved 1.1.43 DLL can be rebuilt byte-for-byte. The original DLL
source revision and command were not preserved: that exact-rebuild gap remains
explicitly blocked rather than being filled with a guessed recipe.

### Screen picture and tracked gun in one projection

With the gun enabled in screen mode, the presenter submits a single opaque
projection layer with two views. Each image contains the picture rectangle at
the existing screen pose and dimensions, then the tracked model and aim marker
with a shared depth buffer. A nearer gun covers the picture; a gun behind it is
occluded. The marker sits just in front of the screen and remains depth tested.
The fixed gun targets use the runtime's recommended per-eye dimensions. Screen
sampling uses linear magnification and trilinear mipmapped minification.
Missing controller tracking still displays the picture in that projection.

With the gun disabled, the existing two-quad path is retained. Native no-loader
replay compares both published eye images and the fixed-target quad blits with
the preceding build: zero changed pixels. A synthetic controller in the same
native GL replay covers more than fourteen thousand picture pixels in each eye
when nearer, and zero when behind. A CPU resampling of the former quad mapping
correlates above 0.98 with the minified picture (filtering differs); this is an
offline mapping/sharpness comparison, not compositor or headset acceptance.
All images and game dumps remain private. Bounded image waits are unchanged;
both eye pictures are sampled from one publication and fenced before reuse.

### Optional runtime eye separation in screen mode

`PCSX2_STEREO_RUNTIME_IPD=1` opts screen mode into the Euclidean distance
between the two valid located OpenXR view positions. It is off by default;
the configured separation (normally 0.063 m) stays the reference otherwise.
The GS thread latches one value per emulated frame for both eyes and every
draw. Invalid/non-finite positions, an implausible baseline, a backward clock
or a sample at least 500 ms old retain the configured value when latching the next frame. INSIDE keeps its
existing pose-based path and does not use this override. No per-game values
or launchers are changed.

`runtime_ipd_test.cpp` compiles against the applied source's `pcsx2` include
folder with an ordinary C++17 compiler. Run it in separate processes with
`PCSX2_STEREO_RUNTIME_IPD=0` and argument `0`, then environment value `1` and
argument `1`. It tests synthetic 72/64 mm view pairs, rigid rotation/translation,
frame latching, stale/invalid/reset fallback and unchanged INSIDE selection.
The native dump proof also compares the opt-in synthetic pair with the same
fixed separation and compares the default-off output with the preceding build.
`PCSX2_STEREO_TEST_IPD_M` is a test-only injection guarded by this opt-in,
screen mode and `PCSX2_OPENXR=0`; it is ignored with a runtime active. These
synthetic poses are not headset measurements or physical-comfort acceptance.


### Repeating the no-runtime screen/gun proof

`screen_proof_test.py <private-capture-directory>` validates a matched replay
triplet with Pillow and NumPy. Capture `baseline/` with the preceding build and
no gun, `no-gun/` with the candidate and no gun, and `combined/` with the
candidate's existing `PCSX2_VR_SCREEN_PROOF` synthetic poses. Keep the same
combat dump, profile and save frame across the triplet; explicitly disable
OpenXR, use hidden bounded runs at BelowNormal priority, and keep every image
and run receipt in ignored private storage. The validator never launches a
process or loads a runtime.

Each folder supplies `run.log.json`, `live.log`, the `live-shared-{eye}.png`
and `live-blit-{eye}.png` readbacks. The combined folder also supplies
`screen-{eye}-{picture,front,behind,dot}.png` and
`screen-screen-order.json`. The production renderer uses a picture plane at
2.5 m, synthetic near/behind grip poses at 0.42/4 m, and a 63 mm eye baseline.
The validator requires exact no-gun eye/blit equality, opaque picture pixels
covered by the near gun, zero changed pixels for the behind gun, an aim marker,
one opaque projection layer, successful draws and clean exits with no GL
errors. Repeat per combat dump rather than inferring per-game acceptance from
one scene. These checks establish native offline composition only; tracking,
compositor behavior and headset comfort remain separate checks.

### GunCon 2 calibration and INSIDE picture breakup (Time Crisis 3)

Owner report: in INSIDE mode the Time Crisis 3 GunCon 2 calibration could not
be passed and the picture broke up. Offline reproduction found three causes.

1. Calibration protocol, all modes. Namco's calibration shows one black frame
   after the shot and waits for the gun to report position (0,0), as a real
   light sensor does on a black picture. The VR trigger never produced that
   report, so the screen stayed on "Calibrate 1P side" in screen mode as well.
   The gun now emulates the light sensor: the GS thread marks a displayed frame
   dark when every shown circuit is a pending black clear (or nothing is shown
   and BGCOLOR is black); the GunCon 2 then reports (0,0) for that frame. No GPU
   readback is used. `PCSX2_VR_GUN_LIGHT_SENSE=0` disables it.
2. INSIDE aim parallax. A ray that hits no retained world triangle (every 2D
   screen, including calibration) was projected from a point 100 m along the
   controller ray. A gun held below the eyes then reported a point well above
   the target, and the game rejected the calibration shot. Such rays now map
   where they cross the stationary picture/HUD panel, the same quad mapping as
   screen mode. World-triangle hits are unchanged. `PCSX2_INSIDE_GUN_PANEL=0`
   restores the old far point for comparison.
3. INSIDE double reprojection. Time Crisis 3 builds its depth-of-field by
   resampling a copy of the finished frame in small tiles. Those draws were
   classified as HUD and placed on the panel, which re-projected an already
   reprojected eye picture: a duplicate enemy, ghost layers and a missing cover
   rock. With `PCSX2_INSIDE_SCREEN_EFFECTS=1` (per profile), non-world draws
   on the camera target that sample a render target or a same-size frame copy,
   and untextured fills covering the whole scanout, stay in picture space. This
   also enables the scanout-aware policy above, so large world batches stay
   reprojected.

`PCSX2_VR_GUN_SCRIPT_SPACE=hand` models a gun held at
`PCSX2_VR_GUN_SCRIPT_HAND_X/Y/Z` (default 0.18, -0.30, -0.25 m from the eyes)
whose barrel points at what the viewer sees at picture (u,v): the screen quad
in screen mode, or the nearest world triangle, else the panel, in INSIDE mode.

Offline results (OpenXR disabled, hidden runs; pictures and logs private):
screen mode with the hand script now passes calibration ("Is the gunsight
aligned correctly?"); with light sensing disabled it does not, and two
identical no-shot runs give identical pictures. The game's own sight marker
lands within 0.0075 of the picture size of six aimed points in screen mode and
in INSIDE mode with the panel mapping. INSIDE with the old far-point mapping
reports (0.461, 0.413) for a centre shot and the game rejects the calibration.
INSIDE at a 20 degree fake head turn also passes. Combat replays at a 30
degree turn lose the duplicate enemy and ghost layers and regain the blurred
cover rock; exact-zero INSIDE still equals screen mode in every RGBA pixel.
Remaining limits: original-frustum holes, temporal effects, physical tracking,
real hand offsets and headset comfort are not established offline.

Crisis Zone and Ninja Assault draw their black frame with ordinary draws, so a
pending clear is not enough. For 45 frames after each VR trigger edge the GS
thread also reads back five 8x8 blocks of the displayed picture (centre and
the four quarter points); the frame is dark when every sampled channel is at
most 16. Outside that window only the free clear/blank test runs. The
readback adds a GPU sync on those frames; live speed with it is unmeasured.

Other gun profiles, same offline method (INSIDE, exact-zero pose, hand script,
calibration shot at the centre, then six held aim points measured against the
game's own sight marker): Time Crisis II accepted, max error 0.0031; Ninja
Assault accepted, 0.0088; Crisis Zone accepted, 0.0149 before and 0.0041 after
a vertical gun scale of 1.045 in its private profile; Virtua Cop's calibration
crosses stay within 0.0015. Vampire Night shows no calibration screen at boot
and was not measured. With `PCSX2_INSIDE_SCREEN_EFFECTS=1`, combat replays at a
30 degree turn change no pixels for Crisis Zone, Vampire Night, Ninja Assault
and Virtua Cop in the sampled scenes; Time Crisis II loses a darkening panel
edge, and Time Crisis 3 loses its duplicate scene.

### INSIDE head-movement smear and the upscale cap (Time Crisis 3)

Owner report: in INSIDE mode Time Crisis 3 showed stereo artifacts when the head
moved, and the frame rate was poor; the stereo itself looked right with the head
still. Code review found four causes.

1. Wrong pose on the published pair. Each eye picture was sent to the compositor
   with the newest latched head pose. A game that shows a buffer finished in an
   earlier frame, or repeats a picture, was then reprojected by the compositor
   against a pose that did not draw it, so the world swam or doubled while the
   head turned. Each eye now records which latched pose drew each render target,
   the GS scanout notes which target it shows, and the pair is published with
   that drawing pose (kept in a 64-entry history). `PCSX2_INSIDE_LEGACY_POSE=1`
   restores the old newest-pose publication for comparison.
2. The profile upscale cap was ignored. `PCSX2_INSIDE_MAX_UPSCALE=2` still ran at
   3x: the chooser never returned less than 3, the 2-second check forced 3x
   again, and the Qt host accepted only 3 or 4. Caps 1 to 4 now apply.
3. Per-draw environment reads. The INSIDE classifier read about fifteen
   environment variables per draw per eye on the GS thread. They are read once.
4. Nearest-neighbour eye stretch. The eye picture was stretched to the headset
   grid without filtering. A scaled blit is now linear; a 1:1 blit stays exact.
   `PCSX2_VR_NEAREST_BLIT=1` restores nearest.

Offline measurement: with OpenXR disabled, INSIDE and
`PCSX2_INSIDE_TEST_HEAD_YAW_DEG` set, a synthetic presenter publishes a sine
head turn at `PCSX2_INSIDE_TEST_HZ` and blits both eyes like the headset path.
The live log prints `Stats` lines every `PCSX2_LIVE_STATS_SECONDS` with speed,
GS thread time and use, game frame rate, and pose agreement counters (stale
pairs, repeated pictures, eyes drawn with different poses, pictures drawn with
two poses), plus `Pose totals` at close. `inside_pose_test.py` checks the cap,
the pose history and the drawn-pose bookkeeping on the CPU, and pins the wiring.
