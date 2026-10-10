# libacvr shared runtime core — first implementation

The MIT library now implements the common frame loop behind the published C
exports. `src/runtime_host.hpp` is a private C++ platform boundary, not a second
public ABI. A recorded host drives tests; a production OpenXR host is still
required. Public `acvr_runtime_create` returns `ACVR_UNSUPPORTED` and clears its
output until that host exists. No test starts a graphics device or XR runtime.

## Implemented

- One owner thread, copied config strings/API table and deterministic cleanup.
  The backend closes before the host/device is destroyed.
- Native rational-rate scheduling independent of display frequency, bounded
  catch-up with retained backlog, and wall-deadline reset after a 250 ms gap.
  Native simulation time/tick IDs never jump to conceal missed work.
- One immutable lease across both eyes and display replays. The old lease is
  used for gun aim, then released before inputs and the next native step.
- Single gun aim from a predicted controller pose: scale once, nearest-hit
  coordinates or camera/far-ray projection, raster/viewport off-screen checks
  and previous-frame aim provenance. Shared muzzle math supports a bind-pose
  muzzle offset and calibration rotation without any visual recoil transform.
- Digital transitions queued across faster display frames and consumed once
  per native tick. Pause/focus/tracking loss clears stale transitions; trigger
  recovery requires release before another shot. Lost gun tracking also
  neutralizes its player's cover pedal.
- Output draining once per native step, ordered sequences, overflow-triggered
  effect cancellation and a bounded guard against a broken infinite MORE loop.
- Exactly one end call per successful host begin. Failed right-eye draw submits
  zero layers; runtime failure cancels effects and rejects later ticks.
- Tracking inspection preserves top-level and nested caller prefixes. Pause,
  inspection, ticking and destruction enforce the owning thread.
- A required per-display rigid `scene_from_stage` pose carries the seat anchor.
  Its translation is metres before scene scaling. The host composes both eye
  matrices with the same anchor snapshot; the core applies it to gun rays.
  Reported tracking stays stage-space. Invalid anchors fail before stepping.

## Native gun data reader

The optional `acvr_gun_assets` target decodes an in-memory GLB into named bind
transforms, materials and CPU vertex/index arrays. Set `ACVR_JSON_INCLUDE_DIR`
to an existing nlohmann JSON include directory; CMake never downloads it. CI
reuses the dependency already configured for the Hub. Omitting the option builds
only the dependency-free core and its three tests, not the gun reader.

The reader accepts self-contained rigid glTF 2.0 triangles, float positions and
normals, normalized vertex colours, integer indices and interleaved accessors.
It retains material roles and LOD membership, requires an identity grip root and
an independent static muzzle, and bounds input size, hierarchy, decoded vertices,
primitives and triangles. Malformed/external buffers and unsupported required
extensions fail without replacing the caller's existing asset. This is runtime
data decoding, not the lane's complete authoring/metadata acceptance checker.

With `ACVR_TOML_INCLUDE_DIR` pointing to the existing tomlplusplus include tree,
`acvr_gun_models` adds bounded native UTF-8-path GLB/TOML loading and motion
preparation. One configured gun slot is accepted by the private host path when
the host explicitly supports gun drawing. Config strings/tints are copied;
metadata IDs and grip/muzzle names must agree. Aim uses the tracked grip and
static muzzle; the compatibility path still uses controller aim. The host gets
one immutable model draw packet after each world eye, using that eye's depth.

Named motion events now drive slide/rotate transforms, LOD aliases and finite
recoil decay without changing static aim/reference nodes. Preview injection
rejects duplicate sequences, future ticks, missing nodes and invalid values;
pause, failure and tracking loss clear motion. Models can be hidden while the
laser mode and ballistic ray remain independent. Only LOD0 is currently selected.
This prepares actual render commands; a GPU gun renderer is still required.

## Explicitly incomplete

No OpenXR loader/session/swapchains, Vulkan/GL device provider, model renderer,
control-set parser, recenter/height/pause overlay, or real backend
integration exists yet. The core rejects controls-file requests, hand switching,
multiple-gun or separate-HUD backends instead of ignoring those requirements.
Builds without the optional model dependencies reject configured model slots.
The internal host supplies already composed eye matrices and receives scene
scale/anchor configuration; the production provider must implement that math
and device lifecycle. The recorded host does not establish graphics correctness.
Two-gun join policy, automatic input/output-to-motion mapping, haptics and
distance-based LOD selection remain required next slices. Explicit preview
events are not proof that real-game recoil outputs have been mapped.

## Checks

Build `libacvr/` with CMake and run its CTest suite. The C and C++ ABI checks and
the runtime suite require no third-party packages or content. Runtime cases
cover 901 display samples at 90 Hz against 60 Hz native timing, the 59906/1000
rational rate, catch-up limits, interruption reset, short trigger/coin taps,
aim provenance and metre scale, focus/hand loss between native ticks, zero-layer
frames, failed step/output/right-eye draw/flush, cross-thread rejection, malformed
times/axis values and rotated muzzle transforms. These checks prove the common
code path only; the actual XR provider and real game remain separate gates.
With the existing JSON include configured, a fourth suite exercises synthetic
GLB decoding, index widths, interleaved/normalized colours, hierarchy/muzzle
constraints, every truncated prefix, malformed offsets and resource budgets.
No exported gun model or rendered image has been accepted by these tests.
With TOML configured, a fifth suite covers metadata/motion and native synthetic
file loading. The runtime suite also exercises configured left-hand grip aim,
copied config lifetime, both-eye draw order, motion/aim separation, tracking
loss/rearm and model-draw failure. A dependency-free build still passes three
suites. No player-side Python process is involved in model loading.
