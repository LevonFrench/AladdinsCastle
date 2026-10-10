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

## Explicitly incomplete

No OpenXR loader/session/swapchains, Vulkan/GL device provider, model loader or
renderer, control-set parser, recenter/height/pause overlay, or real backend
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
