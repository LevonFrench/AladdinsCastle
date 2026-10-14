# PCSX2 stereo experiment

The runner can capture a GS dump without a window, replay it with OpenGL, and
write a side-by-side PNG. This is a frozen-dump stereo prototype. Headset output
and gun aiming are deferred.

Checkpoint: Virtua Cop replay produced a pair with near/far disparity, matching
opaque HUD interiors and exact stereo-off RGBA equality. That picture precedes
the final depth-test guard, which builds but has not been replay-validated.
OutRun 2006 capture stopped after three startup-dialog pictures; no race dump
or racing stereo pair has been proved.

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

For eligible Q-less untextured triangles, replace Q with `(Z-A)/B`. Shift only
varying-depth, depth-tested triangles; perspective-textured triangles also need
varying Q. Sprites, FST draws, constant depth and untested screen overlays are
pinned. Conservative classification can leave some world geometry flat.

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
| `PCSX2_STEREO_TRACE` | Optional native CSV path for actual draw vertices |

Run the entire frozen dump twice in separate fresh processes, one per eye. Each
has its own GS device, cache, color/depth targets, temporary targets and feedback
history. Main and off-screen draws use the same displacement policy. This
provides complete paired target lifetimes for replay; it is not live multiview.
Both eyes begin from the same mono dump snapshot, so prior-frame effects require
additional investigation. Join matching eye frames inside the runner with
`-sbs <native-left-png> <native-right-png> <native-output-png>`.

Trace columns include Q, Z, primitive expansion, texturing, mode and depth-test
state. Fit `Z=A+B*Q` on eligible perspective vertices, record residuals, and reject
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
opaque interiors or the HUD geometry, not entire translucent rectangles.

Shader displacement does not yet update CPU texture-cache coverage or every
feedback-copy rectangle. Wide shifts and edge effects need further testing.
Frozen screenshots cannot certify temporal motion blur, bloom or speed streaks
across a full game. Headset work will need synchronized live eye targets, metric
and FOV calibration, off-axis presentation and effect testing. No VR services are
changed. Silent Scope scope analysis is skipped for this assignment.

No disc, BIOS, dump, picture, executable, log or operator pathname belongs here.
Run `tools/check_public_privacy.py` before each commit; do not push this experiment.
