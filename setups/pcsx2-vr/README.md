# PCSX2 stereo experiment

The runner can capture a GS dump without a window, replay it with OpenGL, and
write a side-by-side PNG. The live Windows OpenGL frontend now renders both eyes
and has an in-process OpenXR stereo-screen presenter. Gun aiming is deferred.
The OpenXR submission path is compiled but still requires headset testing.

Virtua Cop and OutRun 2006 now have replay-validated pairs with near/far disparity,
exact stereo-off RGBA equality to unchanged replay, and exact HUD equality in a
diagnostic that suppresses displaced world draws. The depth-test guard pins
Virtua Cop's lock-on marker at zero disparity while retaining world depth.

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
handoff to a shared, surfaceless WGL context on the presenter's own thread.
The screen presenter adapts the ACVR implementation pinned in
`upstream-pin.json`; no VR controller input or camera tracking is added.
`PCSX2_OPENXR=1` runtime-loads the existing `openxr_loader.dll` and creates two
OpenGL swapchains, matching the published dimensions. Each is submitted as a
quad visible only to its corresponding eye. The GL-to-quad copy flips rows;
actual runtime orientation remains a headset acceptance check.

The screen is placed in LOCAL space ahead of the initial head position/yaw.
Home recentres it. Its width is distance times native image width divided by
`PCSX2_STEREO_FOCAL_PX`; height follows the display aspect. Thus screen size uses
the profile's game FOV, which remains provisional rather than calibrated.
`PCSX2_OPENXR_DISTANCE_M` defaults to 2.5 and `PCSX2_OPENXR_ASPECT` to 4/3.

Portable live runs use `-portable -nogui -fastboot <native-disc-copy-path>` and
`PCSX2_LIVE_HIDDEN=1`, with their own fresh configuration, BIOS and disc copies.
Use OpenGL, `upscale_multiplier=3`, null/muted audio and native XInput default
mapping for pad 1. Do not inherit an existing emulator configuration. Memory
cards may remain disabled for menu/attract experiments. Escape requests graceful
shutdown. `PCSX2_LIVE_STOP_FILE` supplies an optional private file-based shutdown
request. Launchers require an already-running runtime; they must never start or
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
remain untested. The two measured game profiles above remain provisional; other
launchers use an unvalidated generic Q-only profile. They do not constitute
whole-game stereo acceptance.
