# Publish presenter: Model 2 first

This is a separate GPL-3.0-or-later library, extracted from the existing Model 2
presenter with its notices preserved. The existing MIT `acvr` target and public
C ABI are separate. The new target is not linked into `acvr` by default.

The emulator owns its time and publishes completed FBO pairs. The library owns
the hidden shared WGL context, loader table, session loop, fixed swapchains,
10 ms image waits within a 250 ms budget, GL error drain, ownership fences,
screen quads and stamped opaque projection images. It also owns screen placement,
controller actions/haptics, a 250 ms input lease and the shared hold-to-quit policy.
Game-specific bitmap transport and button mapping remain in the Model 2 viewer.

`Presenter.h` is the small C++ entry point. `start`, `publish` and `stop` require
the producer's owning context as documented there. Stop failure preserves the
resources for a later retry on their owning context. One session is supported.
Fences protect both ownership transfers; Model 2's conservative completion
points are retained during this first migration. Runtime image waits never hold
the publication mutex. Encoded pixel values and the original two-quad screen
geometry are retained.

Projection clients can opt into `located_views`, render using those LOCAL poses
and FOV, then call `publish_projection` with the exact packet used for that pair.
Poses and pixels share one protected publication and serial. Re-showing a pair
uses its stored poses, rather than newer poses. Invalid packets are rejected.
Model 2 never opts in and makes no extra locate-view calls in its screen path.
Projection and alpha-layer builders are also independently testable in `Layers.h`.
Separate HUD publication and each future emulator's expanded input mapping still
need their own adapters and proofs; no Model 3 or PCSX2 client is migrated here.

Build with the existing local compiler and headers through the Model 2
`build-viewer.ps1 -SharedPresenter` switch. It builds a static archive and links
`model2-viewer-shared.exe`. The default switch-free build uses the byte-preserved
`presenter/legacy` snapshot and still produces `model2-viewer.exe`, so both can
be built in one output folder. Nothing replaces an installed viewer automatically.

For CMake consumers, opt into `ACVR_BUILD_PUBLISH_PRESENTER` in the parent
project and supply the existing `ACVR_OPENXR_INCLUDE_DIR`, or configure this
directory directly with that include variable. Nothing fetches dependencies.

`test-presenter.ps1` runs the existing hidden/no-loader GPU harness against the
adapter and shared implementation, plus `shared_test.cpp`. The GPU oracle checks
48 exact pairs, repeated resizes, stamped poses/serials, producer reuse fencing,
restart and context-loss cleanup. Injected tests cover the loader table,
session/focus ownership, stale input, GL drain cap, both layer types and recentre.
Use `-LegacyPresenter` to run the preserved harness separately. Optional
`-RunGatePath` stops subsequent child builds/runs if the specified marker exists.
These are offline/source proofs. Headset A/B play, comfort, physical input and
compositor behavior remain untested.
# Mono gun theatre client

`TheatreClient.h` adds an opt-in `start_theatre` entry point. A client receives
current predicted eye views, recommended per-eye extents, focused tracked
aim/grip and game buttons on the owning XR GL thread. `update` runs even when
no image should be submitted. `draw_eye` attaches the supplied colour texture
to its own depth framebuffer and completes each eye from the same retained
picture. The shared presenter owns loader/session/actions, finite image waits,
image release and one opaque projection layer; it never selects the quad path
for a theatre client. Callbacks and their resources retire on that same GL
context before session teardown. The separate renderer/GLB parser is shared
source, and the legacy publish/quad APIs retain their original default path.
