# Namco System 22 stereo: CPU foundation

This block proves synthetic scene reconstruction and the published libacvr v0.1
frame/camera/raycast interface. It does **not** run Time Crisis or any other game.
All geometry comes from formulas in the synthetic fixture; no game content,
textures, captures, upstream source or third-party binaries are included.

`acvr_backend_query` reports `supported_graphics=0`; a real libacvr graphics
runtime must reject it. `game_draw_eye` returns `ACVR_UNSUPPORTED` without
drawing unless explicitly bound through the private [GL integration slice](GL-DRAW.md).
`game_open` accepts only `synthetic-system22`, an API-0 test record,
and empty content/storage/options strings. It never opens those paths. The
setup-local `stage_cpu_scene` and `draw_cpu_frame` bridge use the same owned
frame lease, camera and nearest-hit data as the callbacks. The reported synthetic
59.906 Hz rate exercises the contract; there is no emulation or sound scheduler
connected to this factory. The [worker/source-hook slice](LIFECYCLE.md) is separate
scaffolding for the actual host/frame/video seams, with a disabled legacy loader.
No gun controls, persistence, output events or separate-HUD capability are
advertised. The Time Crisis ADC helper is tested math, not register writes.

The CPU core reconstructs x/y at 1/16 native pixel resolution and positive
view-space depth into right-handed +X right, +Y up, -Z forward. It accepts
identity view-local source cameras only. Scene units stay in the supplied depth
units; eye matrices already contain the runtime's anchor and physical scale.
The CPU path uses OpenGL clip space and lower-left eye rectangles, converts
to top-left image rows, clips triangles against all six homogeneous planes,
and depth-tests opaque flat colours or the [owned synthetic material packet](MATERIALS.md).
Material packets sample native tile/pen/palette/shade data per CPU fragment;
homogeneous clipping and new-eye perspective interpolation preserve UVs.
Parallel test cameras use off-axis frusta.
The [owned fog state/decision helper](FOG-STATE.md) preserves original native
depth and copied CZ/mixer state per lease for inspection only; it applies no fog
to pixels and admits no real board or game.

Explicit HUD test tags flatten to one chosen depth and render into a separate CPU
image. Backdrop tags ignore eye translation and sit at infinity; gun-flash tags
are discarded. These tags test policies, **not** a real System 22 layer classifier.
Raycasts include world triangles only, returning nearest distance, original
camera ID and normalized native-raster gun coordinates. Old, partial and extended
`acvr_hit` records preserve caller sizes and unknown bytes. Cabinet ADC clamping
is separate; viewport rejection retains normalized diagnostics.

## Build and test without graphics

Use existing C11/C++17 compilers, CMake and Python 3; no packages, downloads, Qt,
SDL runtime, OpenXR or headset are required. Installed platform GL headers compile
the dispatch-only source; tests call local mocks, with no GL library/context.
For example, in a configured compiler terminal:

```powershell
# Run from the checkout root.
cmake -S setups/namco22-vr -B .local/build-stereo -DCMAKE_BUILD_TYPE=Release
cmake --build .local/build-stereo --config Release --parallel 2
ctest --test-dir .local/build-stereo -C Release --output-on-failure
```

CI uses this standalone CMake project through the lead's shared workflow.
Strict warnings apply to the library, test and capture executables on MSVC/GCC.
Tests cover a cube, asymmetric focal lengths, convergence and disparity,
translated gun/head origins, nearest camera association, clipping, layer policies,
input/lease sequencing, pause/replay, empty-frame camera retention and ABI tails.

`namco22_synthetic_capture` writes a synthetic PPM pair only to a new absolute
`.ppm` path whose parent already exists under the invoking checkout's `.local`.
Run it from that checkout root. It rejects outside directories, existing output
files and symlink leaves; canonical directory checks reject redirected paths.
It reads no geometry input. Build artifacts and captures remain ignored.
The optional `--materials` argument selects a formula-generated checkerboard cube
with owned pen tiles and per-face palettes. It still uses CPU rendering only.

## Real-board integration still required

Time Crisis is first. `profiles/timecris.toml` records the documented projection
fallback and ADC conversion and explicitly leaves physical scale unconfigured.
Rave Racer and other racers follow Time Crisis when directed by the lead; no
racing scale, HUD classifier or camera profile is claimed here.

Follow the Namco22 integration plan in the shared contract: a source-pinned
setup-owned build copy, guarded exact patches, native worker frame-boundary
rendezvous, frozen register/display-list publication, and an actual GL renderer
under `game_draw_eye`. Do not invoke the nonreturning game entry as `game_step`.
The compiled GL slice reuses this callback and lease; dispatch tests run mocks
only. It requires shared forward depth and a desktop GL3.3 compatibility provider.
The [GL material path](GL-MATERIALS.md) uploads exact bounded synthetic rectangles
once per immutable lease, replays both eyes and cleans only its own textures on
owner-context release/close. Capacity, driver pixels and native composition remain
separate acceptance gates; no real graphics provider is enabled here.
Capture focal/centre/camera association at emission before sorting; preserve
direct screen-space primitives, mixed sprite priorities, textures, fog and gamma.
The existing reference clones stay read-only. This block neither downloads
upstream. The follow-on hook slice applies guarded scaffolding only to an ignored
source overlay and verifies it against the existing pinned source; see LIFECYCLE.md.

Open gates include actual game initialization, input/register/audio/output
integration, real layer classification, camera/cull expansion, near-face comfort,
GL resources/synchronization and per-game world-scale calibration. The CPU core
has no texture rasterizer or native composition parity. It can reconstruct only
submitted polygons; missing geometry on a head turn remains unresolved.
Attract-loop/stage depth and native calibration, racers, Hub install/launch and
headset play all require separate owner-approved real runs. Board 1 is not done.

Reuse on later boards: owned prepared frames, callback validation, original
camera provenance, CPU matrices/clipping/depth tests and synthetic fixtures.
Model 3 should use model-space replay; it must not inherit the System 22
unprojection or native LOS assumptions.
