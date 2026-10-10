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

## Explicitly incomplete

No OpenXR loader/session/swapchains, Vulkan/GL device provider, model renderer,
control-set parser, recenter/height/pause overlay, or real backend
integration exists yet. The core rejects model/controls-file requests and
multiple-gun or separate-HUD backends instead of ignoring those requirements.
The internal host supplies already composed eye matrices and receives scene
scale/anchor configuration; the production provider must implement that math
and device lifecycle. The recorded host does not establish graphics correctness.
Two-gun join policy and animation/haptic mapping remain required next slices.

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
