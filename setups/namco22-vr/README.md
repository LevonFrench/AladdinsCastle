# Namco System 22 stereo: CPU foundation

This block proves synthetic scene reconstruction and the published libacvr v0.1
frame/camera/raycast interface. It does **not** run Time Crisis or any other game.
All geometry comes from formulas in the synthetic fixture; no game content,
textures, captures, upstream source or third-party binaries are included.

`acvr_backend_query` reports `supported_graphics=0`; a real libacvr graphics
runtime must reject it. `game_draw_eye` returns `ACVR_UNSUPPORTED` without
drawing. `game_open` accepts only `synthetic-system22`, an API-0 test record,
and empty content/storage/options strings. It never opens those paths. The
setup-local `stage_cpu_scene` and `draw_cpu_frame` bridge use the same owned
frame lease, camera and nearest-hit data as the callbacks. The reported synthetic
59.906 Hz rate exercises the contract; there is no emulation or sound scheduler.
No gun controls, persistence, output events or separate-HUD capability are
advertised. The Time Crisis ADC helper is tested math, not register writes.

The CPU core reconstructs x/y at 1/16 native pixel resolution and positive
view-space depth into right-handed +X right, +Y up, -Z forward. It accepts
identity view-local source cameras only. Scene units stay in the supplied depth
units; eye matrices already contain the runtime's anchor and physical scale.
The CPU path uses OpenGL clip space and lower-left eye rectangles, converts
to top-left image rows, clips triangles against all six homogeneous planes,
and depth-tests opaque flat colours. Parallel test cameras use off-axis frusta.

Explicit HUD test tags flatten to one chosen depth and render into a separate CPU
image. Backdrop tags ignore eye translation and sit at infinity; gun-flash tags
are discarded. These tags test policies, **not** a real System 22 layer classifier.
Raycasts include world triangles only, returning nearest distance, original
camera ID and normalized native-raster gun coordinates. Old, partial and extended
`acvr_hit` records preserve caller sizes and unknown bytes. Cabinet ADC clamping
is separate; viewport rejection retains normalized diagnostics.

## Build and test without graphics

Use an existing C++17 compiler and CMake; no packages, downloads, Qt, SDL, GL,
OpenXR or headset are required. For example, in a configured compiler terminal:

```powershell
$checkout = 'J:/projects/games/aladdinscastle/.local/worktrees/stereo-boards'
cmake -S "$checkout/setups/namco22-vr" -B "$checkout/.local/build-stereo" -DCMAKE_BUILD_TYPE=Release
cmake --build "$checkout/.local/build-stereo" --config Release --parallel 2
ctest --test-dir "$checkout/.local/build-stereo" -C Release --output-on-failure
```

CI uses this standalone CMake project; shared workflow wiring belongs to the lead.
Strict warnings apply to the library, test and capture executables on MSVC/GCC.
Tests cover a cube, asymmetric focal lengths, convergence and disparity,
translated gun/head origins, nearest camera association, clipping, layer policies,
input/lease sequencing, pause/replay, empty-frame camera retention and ABI tails.

`namco22_synthetic_capture` writes a synthetic PPM pair only to a new absolute
`.ppm` path whose parent already exists under the invoking checkout's `.local`.
Run it from that checkout root. It rejects outside directories, existing output
files and symlink leaves; canonical directory checks reject redirected paths.
It reads no geometry input. Build artifacts and captures remain ignored.

## Real-board integration still required

Time Crisis is first. `profiles/timecris.toml` records the documented projection
fallback and ADC conversion and explicitly leaves physical scale unconfigured.
Rave Racer and other racers follow Time Crisis when directed by the lead; no
racing scale, HUD classifier or camera profile is claimed here.

Follow the Namco22 integration plan in the shared contract: a source-pinned
setup-owned build copy, guarded exact patches, native worker frame-boundary
rendezvous, frozen register/display-list publication, and an actual GL renderer
under `game_draw_eye`. Do not invoke the nonreturning game entry as `game_step`.
Capture focal/centre/camera association at emission before sorting; preserve
direct screen-space primitives, mixed sprite priorities, textures, fog and gamma.
The existing reference clones stay read-only. This block neither downloads
upstream nor applies an engine patch.

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
