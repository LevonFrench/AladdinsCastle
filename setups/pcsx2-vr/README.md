# PCSX2 stereo experiment

The runner can capture a GS dump without a window, replay it with OpenGL, and
write a side-by-side PNG. This is a frozen-dump stereo prototype. Headset output
and gun aiming are deferred.

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
OpenGL stereo, diagnostics and the SBS writer. Build the unchanged frontend and
runner first and preserve their binaries/resources separately. Build each
project's own `.vcxproj` with `/t:Build`, Release/x64, v145,
`/p:VCToolsVersion=14.51.36231`, `/p:PreferredToolArchitecture=x64` and
`/p:SolutionDir` set to the source root **with a trailing backslash**. Build the
patched runner the same way. Use a portable private data directory.

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
provides complete paired target lifetimes for replay; it is not live multiview.
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
It instead uses OpenGL and independent full replays, avoiding selective
promotion of display-chain targets. Actual vertex logging and residual checks
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
across a full game. Headset work will need synchronized live eye targets, metric
and FOV calibration, off-axis presentation and effect testing. No VR services are
changed. Silent Scope scope analysis is skipped for this assignment.

No disc, BIOS, dump, picture, executable, log or operator pathname belongs here.
Run `tools/check_public_privacy.py` before each commit; do not push this experiment.
