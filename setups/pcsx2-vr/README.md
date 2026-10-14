# PCSX2 stereo experiment

Status: unchanged emulator and GS runner build successfully. The unattended
capture patch compiles, but capture has not succeeded. No GS dump, replay PNG,
stereo renderer or accepted side-by-side pair exists. Headset output and gun
aiming are outside this work.

## Source and dependency pin

`upstream-pin.json` pairs PCSX2 master commit
`904bd9da01c68bb31ae86f4ea6e0e9a1fb0743e2` with the preserved official Windows
archive's exact size and SHA-256. The source recipe and archive headers agree on
Qt 6.12.0, FFmpeg 9.0.1 and SDL3 3.4.18. No per-release-tag dependency pack exists.
The rolling archive is overwritten on rebuild: preserve the recorded bytes
instead of downloading latest again. The earlier stable header workaround is
removed; that stable-tag experiment is historical.

Unchanged frontend and runner built outside the sandbox with v145, the same
explicit tool version and x64 host tools. Build each project's own `.vcxproj`
with `/t:Build` and `/p:SolutionDir` set to the source root with a trailing
separator. Keep runs hidden, bounded, below normal and silent. Preserve unchanged
binaries separately for the eventual stereo-off comparison.

## Compiled capture patch, pending runtime acceptance

Apply `unattended-capture.patch` to the pinned source. Its runner arguments are:

- `-capture-frame <N>`: presentation frame from 1 to 36000.
- `-capture-output <output.png>`: internal PNG and uncompressed GS dump.
- `-capture-bios <private-ROM0-copy>`: copied BIOS, with copied disc as the final
  positional argument.

Capture forces a surfaceless renderer, fast boot, null/muted audio, disabled
input sources, memory cards and network devices. A GS-thread frame counter queues
the snapshot/dump. An atomic completion flag lets the CPU drain work and stop
twelve presentations later. Startup errors are printed directly to stderr.
Ordinary dump replay remains available without these capture arguments.

Use **native Windows backslashes** in file arguments. PCSX2 uses
`PathCchCanonicalizeEx` with an extended-path flag; forward-slash drive paths are
retained inside the extended prefix and fail `GetFileAttributesW`. A read-only
native API reproduction found the backslash spelling resolves the same file.

Three capture runs failed before a frame was produced. The first two lacked an
actionable console error; the third returned "Requested filename ... does not
exist". The launcher's forward-slash argument caused this failure. Work stopped
at the three-failure limit. No fourth game run occurred. The pathname correction
has filesystem evidence, not successful game-capture evidence.

## PenguinScreen2 source study and planned reuse

[PenguinScreen2](https://github.com/PenguinVRLab/PenguinScreen2) supplies GS vertex
stereo parameters and implements its active geometric shift in the Vulkan
shader. Multiview chooses the eye sign. Q drives linear, logarithmic or piecewise
depth maps; uniform-Q perspective draws can be pinned to the screen. An optional
Z/Q estimator derives inverse depth from varying textured, depth-tested draws.
Its clamped linear formula differs from the signed parallel-camera pixel
displacement required here.

Its texture cache promotes display-chain targets into two layers, follows
render-to-texture feed edges, matches depth-target layer counts and retargets
source views. Temporary depth and right-eye DATE stencil operations have known
gaps. At the reference pin, OpenGL declares stereo fields without applying the
geometric shift; Vulkan provides multiview target support.

Planned reuse, credited to PenguinVRLab, is Q-driven displacement, conservative
screen-space classification and evidence-based Z fallback. Separately licensed
bundled profiles are not reused. Initial OpenGL work would replay the complete
frozen dump independently for each eye, preserving every intermediate color,
depth and feedback history through separate target/cache lifetimes. This is a
plan; no stereo code or visual acceptance exists yet.

## Remaining acceptance

On renewed resume, capture with native Windows paths. Replay the dump surfaceless
in the unchanged runner to an internal PNG. Then implement environment-controlled
stereo, log actual Q/Z pairs and fit `Z=A+B*Q` with residual checks. Keep sprites,
constant depth, HUD and text at zero disparity; keep all intermediate color,
depth and feedback histories per eye. Stereo-off pixels must match the baseline.

Virtua Cop must pass near/far disparity and identical-HUD checks before OutRun
2006 starts. The racing capture must show a race or attract race, report cockpit/
first-person availability, and identify blur, bloom and speed-effect failures.
Per-game constants, images and effects remain unmeasured.

All game, BIOS, dump, log, image and executable artifacts stay private in the
ignored job directory. Run `tools/check_public_privacy.py` before every commit.
This setup includes no game/BIOS data and changes no VR services.
