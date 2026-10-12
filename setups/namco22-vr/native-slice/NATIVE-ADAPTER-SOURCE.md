# Private native adapter source

Separate default-OFF `ACN22_BUILD_NATIVE_ADAPTER` consumes the lead's reviewed
v0.4 appended `game_prepare_display` contract. The production query stays mask0
with null preparation; the accepted SDL proof executable/scene/oracle/interface
stay frozen. This adapter is source-only and uncompiled at this checkpoint.

The private factory accepts the old complete prefix with graphics mask0, or a
full current table with GL and its mandatory preparation callback. Partial tails
are rejected and caller prefix/tail bytes preserved. No public backend is repinned.
Open admits only the two existing synthetic fixtures, empty content/storage and
no controls/guns. It borrows an existing GL3.3+ compatibility WGL owner context,
resolves through runtime `get_proc`, and claims one native instance. No context,
window, session, library loader, emulation worker or presentation is created.

Step owns sorted native quads/resources/camera and publishes one lease; release
invalidates packet identities before freeing CPU source. Preparation receives two
stamped one-view records with distinct colour/FBO targets (and distinct depth
handles when supplied), validates both,
stages both packet lists, checks materials/resources, primes native resources and
locks bakes to hits. Borrowed targets/pointers are not retained. Eye identity is
copied/compared by declared fields and matrix bits, without C padding. Private
target/cache FNV fingerprints are correlation checks, not cryptographic receipts.

Each draw revalidates its fresh target/lease/source/cache and expected eye order,
draws native packets into that FBO, and flushes native batching before state/target
restoration. Its call-local guard preserves every borrowed FBO draw-buffer selection
on success and failure, restoring that target before entry FBO restoration. Colour
attachments must use mip level0; unsupported attached levels fail before level0
dimension/format queries. The GL state scope preserves the caller and default VAO, legacy client
arrays, server texture/matrix/env state, sampler/program/buffer bindings, FBO/buffers,
pixel transfer/store/PBO state and masks/enables. Unproved imaging transforms are
rejected. Colour clears respect the eye rectangle. Depth remains untouched; world
depth and guns are not admitted. No backbuffer readback, swap or runtime queue flush.
Close frees/reset native cache/CZ/batch resources under the borrowed owner context.

Authored C tests cover actual private factory/canaries and no-backend early errors,
two-scene CPU staging, bad-right transactional refusal, shared-target rejection
even with disjoint rectangles, target/display/frame/order drift and unknown eye
tails. The adapter static library and factory test executable now compile/link
with the installed GCC/SDL2 dependencies and current reviewed header. The factory
test passed once: table/canary/function exports and null-backend early errors only,
with no open/step, GL/context initialization or game access. The full-adapter
compile did not build or run the staging/target-state test executables. Existing
standard-library producer tests and pinned patch-boundary tests are separate CPU evidence.
Additional authored fake-dispatch tests cover both target selection lists, failed
selection/draw exits and attached mip rejection; these also remain unexecuted.

Remaining gates: staging/target-state CPU regressions, then a separately
authorized borrowed-context desktop proof
with both-eye FBO/state/flush/replay/resource/lease and close/reopen receipts using
the same frozen scenes/criteria. GL state/error cleanup, in-flight resource lifetime
and native integration behavior remain unverified. Full game composition/capture,
input/output/raycast, world depth/guns, scale, Viewer/compositor/OpenXR/headset gates
remain separate. The lead owns API/core/provider integration and admission.

## Real Time Crisis entry-point gap

`n22_native_adapter` is a static library, not a runnable game renderer. Its private
factory `acn22_native_adapter_query` accepts only `synthetic-system22-native` with
empty content/storage paths; `game_step` builds the two authored formula fixtures.
The existing `n22_native_batch_proof` executable also draws those synthetic scenes.
The production backend still reports graphics mask0 and does not draw a game.
Real Time Crisis needs its live board polygon/camera stream connected to the
native packet/lease path and a caller that opens the private renderer with a
borrowed compatible GL context, prepares both eye targets, and invokes
`game_draw_eye`. This PR supplies neither game loading nor a real-game executable.
No game material or new validation framework is added. GPU/runtime work remains
held until the owner restores availability.
