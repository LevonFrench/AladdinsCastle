# PCSX2 stereo experiment

Status: baseline incomplete. There is no stereo patch or accepted image yet.
No headset presentation or gun aiming is implemented by this experiment.

Current dependency gate: use a Windows dependency pack matched to v2.8.2.
The rolling pack and header-precedence workaround from the historical attempts
below are excluded from resumed builds. The matched archive has not been located
in the official release assets or pinned CI artifact inventories. No resumed
build or game run has occurred. A verified archive URL or supplied local archive
is required before continuing.

`upstream-pin.json` records the stable emulator, reference source and dependency
archive revisions. The stable PCSX2 source was left unchanged. Its Windows
frontend compiled with MSVC v145; the dump runner has not compiled or replayed a
dump. Emulator compilation alone does not establish graphics correctness.

## Baseline build checkpoint

Three bounded build attempts were made; work stopped at the three-failure limit:

1. Sandboxed Release build: `C1902: Program database manager mismatch` in fmt.
2. Same build outside the sandbox: incompatible rolling dependency headers,
   `C2039: 'pix_fmts'` and `'sample_fmts'` are not members of `AVCodec`.
3. Prefer the stable tree's bundled FFmpeg headers through an external MSBuild
   property sheet: core and frontend compiled, followed by
   `MSB4057: The target "pcsx2-gsrunner" does not exist in the project.`

The last failure was a build-target invocation error. The resumed build must use
the runner's own `.vcxproj` with `SolutionDir` set to the source root, outside the
sandbox with v145 only. Once the matched dependency pack is installed privately,
remove the header-precedence workaround. This correction has not been executed.
Do not count this checkpoint as completion of step 1.

The dependency archive is a rolling release, not an immutable pack tied to the
stable tag. It supplies Qt 6.12.0 and libavcodec 63, while the stable tree vendors
libavcodec 62 headers. The compile-only header selection does not validate video
encoding; encoding is outside this experiment.

## Reference study and planned reuse

[PenguinScreen2](https://github.com/PenguinVRLab/PenguinScreen2) adds vertex-buffer
stereo parameters in its hardware GS renderer. Its active Vulkan shader uses
multiview eye signs and Q-based disparity, with linear, piecewise and logarithmic
depth maps. Uniform-Q perspective draws can be pinned to the screen. An optional
Z/Q estimator derives inverse depth from varying textured, depth-tested draws.
The linear shader clamps its result, so it is not this experiment's required
signed parallel-camera pixel formula.

Its texture cache promotes display-chain targets into two layers, follows
render-to-texture feed edges and matches depth-target layer counts. Source views
are retargeted when a target is promoted. Temporary depth targets and right-eye
DATE stencil operations have documented gaps. At the pinned revision, OpenGL
declares stereo fields but does not apply the geometric shift; Vulkan advertises
stereo target support through multiview.

Planned reuse, with PenguinVRLab credit, is Q-driven perspective displacement,
conservative screen-space classification and an evidence-based Z fallback.
Bundled profiles have a separate license and are not reused. Initial OpenGL work
would replay the complete frozen dump independently for each eye, preserving all
color, depth and feedback histories through separate target/cache lifetimes.
That avoids importing display-chain-only target promotion. This remains a plan,
not implemented or visually checked stereo.

Relevant upstream files: `pcsx2/GS/Renderers/HW/GSRendererHW.cpp`,
`pcsx2/GS/Renderers/HW/GSTextureCache.cpp`,
`pcsx2/GS/Renderers/Vulkan/GSDeviceVK.cpp`,
`bin/resources/shaders/vulkan/tfx.glsl`, and
`bin/resources/shaders/opengl/tfx_vgs.glsl`.

## Remaining acceptance

First obtain a hidden chosen-frame dump and replay it surfaceless in the unchanged
runner to a PNG. Then implement environment-controlled stereo and log actual
Q/Z vertex pairs. Fit `Z = A + B*Q`, reject poor fits and preserve zero disparity
for sprites, constant-depth primitives, HUD and text. Use independent targets for
every eye, including intermediate color/depth targets and feedback operations.

Stereo-off pixels must match the unchanged runner. The first gun-game scene must
pass near/far disparity and identical-HUD checks before another game starts. The
first racing test follows immediately after that gate; race scenes must report
cockpit/first-person availability and failures from blur, bloom and speed effects.
Per-game scales, convergence, focal calibration and effects remain unmeasured.

Private game, BIOS, dump, log, image and executable artifacts must remain under
the ignored job directory. Run `tools/check_public_privacy.py` before every
commit. Nothing in this setup supplies copyrighted game or BIOS data.
