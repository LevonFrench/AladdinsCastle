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
- Explicit slot reload levels use the same queued edge/rearm behavior. An
  opt-in private host policy turns a trigger press with a known off-screen aim
  into one `ACVR_GUN_RELOAD` request, suppressing fire for that entire hold.
  Moving back on-screen cannot fire until release/press. Missing initial aim
  evidence does not invent an off-screen reload; the default policy is disabled.
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
Renderer contract v0.2 requires shared depth for a real configured-gun host;
depth-requiring backends fail before drawing a target that lacks it.

Named motion events now drive slide/rotate transforms, LOD aliases and finite
recoil decay without changing static aim/reference nodes. Preview injection
rejects duplicate sequences, future ticks, missing nodes and invalid values;
pause, failure and tracking loss clear motion. Models can be hidden while the
laser mode and ballistic ray remain independent. Only LOD0 is currently selected.
This prepares actual render commands; the compiled GL pass still needs a provider.
Trigger levels now drive declared trigger motion. Successful native fire edges
emit one fallback recoil pulse, then the model packet is refreshed once before
both eyes; display replay cannot restart that pulse. Prepared output routes can
replace this fallback, as described below.
Single-gun hand switching uses the other tracked trigger's rising edge, cancels
old effects and motion, releases an old held trigger, and suppresses the switching
edge. A new release/press is required to fire. The host receives the new hand for
its haptic routing; this callback is not proof of physical haptic execution.

## Prepared output-to-motion routes

The private host may supply `GunOutputRoute` records from resolved control data.
The runtime copies and validates them against the configured slot/player and
actual metadata motion IDs. It does not infer channels or parse controls files.
Duplicate targets, missing motion IDs, invalid amplitudes/durations and wrong
slot/player assignments fail creation. The host must supply verified mappings;
none is supplied for a real game yet.

Matching solenoid/lamp/FFB output levels pulse on a rising edge, at most once per
route/native tick. Explicit event duration expires a level; persistent levels
need an observed low edge. Strength scales the route amplitude, with magnitude
used for signed FFB. FFB STOP is a low level. Display/eye replay never consumes
the event again. The route duration controls visual decay. A declared recoil
route suppresses fallback fire animation even when no recoil event arrives
(such as an empty magazine). An output-owned trigger motion also excludes the
ordinary trigger-level drive.

Pause, handoff, overflow and tracking loss cancel motion and require an observed
low output before rearming. Untracked configured hands do not receive solenoid
or FFB callbacks that would immediately restart haptics. Physical device haptics
and their complete lifecycle remain provider responsibilities. Mounted yaw/pitch
rest at zero angle (clamped to the authored range), including after cancellation
and pulse decay; their normalized input values still interpolate the full range.

## Explicitly incomplete

No OpenXR loader/session/swapchains, Vulkan/GL device provider,
control-set parser, recenter/height/pause overlay, or real backend
integration exists yet. The core rejects controls-file requests,
multiple-gun or separate-HUD backends instead of ignoring those requirements.
Builds without the optional model dependencies reject configured model slots.
The internal host supplies already composed eye matrices and receives scene
scale/anchor configuration; the production provider must implement that math
and device lifecycle. The recorded host does not establish graphics correctness.
Two-gun join policy, control-data parsing and verified real output mappings, haptics and
distance-based LOD selection remain required next slices. Explicit preview
events are not proof that real-game recoil outputs have been mapped.
The optional GL gun renderer now compiles and passes mock-dispatch checks;
it is not yet wired to a production provider or accepted on the GPU. Its
restrictions and manual synthetic test gate are in `docs/libacvr-gun-gl.md`.

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
Single-gun switching tests verify release-before-press, held-trigger suppression,
changed grip rays and hand notifications. Replayed display frames decay rather
than repeat fallback recoil. Missing shared depth rejects the draw and submits
zero layers. Logical trigger-drive tests keep unchanged levels idempotent.
Reload tests cover a short button tap between native steps, explicit tracking
loss/rearm, off-screen composite edges, on-screen re-entry during a held reload,
display replay and the disabled policy. The provider must supply resolved reload
bindings; controls-file loading and actual game consumption remain unverified.
Prepared-output tests distinguish metadata IDs from node/drive names, exercise
amplitude/duration, held levels, same-tick deduplication, explicit expiry, signed
FFB, cancellation/rearm and neutral mounted rest. Runtime integration tests prove
real-route/fallback exclusion, copied routes and no callbacks to an untracked
configured hand. These are synthetic CPU receipts, not game-output acceptance.
