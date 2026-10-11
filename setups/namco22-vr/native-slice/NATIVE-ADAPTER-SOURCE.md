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
restoration. The GL state scope preserves the caller and default VAO, legacy client
arrays, server texture/matrix/env state, sampler/program/buffer bindings, FBO/buffers,
pixel transfer/store/PBO state and masks/enables. Unproved imaging transforms are
rejected. Colour clears respect the eye rectangle. Depth remains untouched; world
depth and guns are not admitted. No backbuffer readback, swap or runtime queue flush.
Close frees/reset native cache/CZ/batch resources under the borrowed owner context.

Authored C tests cover actual private factory/canaries and no-backend early errors,
two-scene CPU staging, bad-right transactional refusal, shared-target rejection
even with disjoint rectangles, target/display/frame/order drift and unknown eye
tails. They are NOT compiled or executed here. Existing standard-library producer
tests and pinned patch-boundary tests were run without native/GL/window activity.

Next gates: independent adapter source review, coordinated compilation and actual
CPU test execution; then a separately authorized borrowed-context desktop proof
with both-eye FBO/state/flush/replay/resource/lease and close/reopen receipts using
the same frozen scenes/criteria. GL state/error cleanup, in-flight resource lifetime
and native integration behavior remain unverified. Full game composition/capture,
input/output/raycast, world depth/guns, scale, Viewer/compositor/OpenXR/headset gates
remain separate. The lead owns API/core/provider integration and admission.
