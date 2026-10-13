# System 22 worker and source-hook plan

Source inspected: `namco22-decompile` commit
`a9f1a0a1f9de62387a7138006fa36933fd477123`. The existing clone is read-only.
`patches/manifest.json` records normalized-text hashes, exact single-occurrence
anchors and a distinct marker for each edit. It also pins the four headers used
for snapshot/geometry interpretation. MIT upstream notices are preserved in
`patches/UPSTREAM-LICENSE`; our worker/bridge/tooling is GPL-3.0-only.

## Native seams and phase

| Pinned source seam | Adaptation |
|---|---|
| `engine/ss22_run.c`, `rr_tick()` (317) | Poll cooperative stop at the slice hook. The original DSP/sound slices and native vblank order remain. |
| Native-frame boundary, inputs/outputs (358–362) | In adapter mode bypass scripted/desktop input, automatic NVRAM/dumps and native output sinks. The permitted input is applied once before C execution resumes; snapshot output bits once per completed native tick. |
| Video/presentation block (366–405) | Copy video registers on the producer; publish and park before the next vblank phase. No prepare/draw/swap/timer on the producer. The owner later calls `ss22_prepare` once from the owned registers. |
| `engine/ss22_video.c`, `ss22_video_prepare()` (50–66) | Route the assembled `ss22_regs` to a copying hook; never publish its mutable board pointers. |
| `engine/geo_hw.c`, emitted `cb(&q, user)` (483) | Capture the full raw/near-clipped/guard-band quad and original `geo_view` before sorting. Its focal/centre are camera metadata; the view/object matrix is not a global camera. |
| `engine/ss22_gl.c`, `push_quad()` (36) | Capture direct polygons with no invented camera. Preserve their flat route. |
| `ss22_prepare()` (110–150) | Its palette and `spot_src` survive into replay. They must point into the lease, including aligned host-order spot words. Polygon callback binding is synchronous and owner-thread-local. |
| Lifted `g->entry()` (546) | C-only trampoline owns the setjmp target; every C++ callback returns before a stop/failure jump. Normal return unwinds surrounding C++ objects. |

The first permit applies tick 1 input before entry. At each completed native
update, video/output publication parks the producer until release plus another
explicit permit. The next input is applied before the pending vblank/native
continuation resumes. Eye draws and display replay never grant permits. Retain
the same native phase when connecting `game_step`; do not run an extra frame
for the right eye or treat `rr_tick()` as an independently callable game step.

## Implemented checks and bridge

`Worker` copies each input packet, rejects skipped ticks, extra permits while
leased, and invalid digital edges. Pause gates new permits at a stable boundary;
resume requires a fresh packet. Cooperative stop wakes an initial/boundary wait,
or a slice poll unwinds the C entry. No thread/process cancellation or exit is
used. Published snapshots remain owned and readable after producer stop.

The optional typed source bridge compiles against the pinned `ss22_regs` and
`geo_quad/geo_view` headers. It deep-copies palette, mixer, characters, text,
sprites, VICS, spot words, polygon RAM, four CZ banks and scalar register arrays.
Owner preparation binds the copied polygon RAM and collects camera-associated
quad copies through the actual hook callback signatures. Source-session admission
is exclusive because the native engine is global, initially one instance per
process. All hook callbacks contain exceptions before returning to C.

Synthetic C entries test permits, held/pressed/released edges, lease rejection,
pause/resume, failed publication, unexpected entry return, stop while waiting and
stop while polling. A C++ destructor check proves the surrounding wrapper returns
normally. Pinned-header fixtures contain only generated arrays and quads; mutating
the source banks after publication leaves the leased copy unchanged. Retained
palette/spot pointers refer to snapshot storage, not temporary preparation arrays.

## Guarded overlay and local source conformance

From the checkout root, with existing Python and an explicitly chosen source:

```text
python -B setups/namco22-vr/tools/patch_engine.py --source <engine-source>
python -B setups/namco22-vr/tools/patch_engine.py --source <engine-source> --output .local/patched-engine-hooks
cmake -S setups/namco22-vr -B .local/build-source -DN22_ENGINE_SOURCE=<engine-source> -DCMAKE_BUILD_TYPE=Release
cmake --build .local/build-source --config Release --parallel 2
ctest --test-dir .local/build-source -C Release --output-on-failure
```

The tool reads exactly the selected engine files/header guards and Git metadata;
it never traverses assets or game directories. No download occurs. It validates
every edit before writing the private overlay. Identical output is checked in
full; modified source/header, mismatched commit, missing/duplicate anchors,
partial-marker output or redirected/outside output aborts. No reference edits.
Patch regressions use synthetic text. Generic worker/guard tests run without the
reference; the optional pinned-header fixture is a separate local evidence gate.

`ACVR_SS22_ADAPTER` is opt-in. **The patched legacy `ss22_main` returns
UNSUPPORTED before its loader or host can run.** This overlay is scaffolding,
not a complete build copy, emulator, install recipe or runnable game adapter.
The source bridge is not wired into the public backend factory yet. The existing
factory still advertises no graphics API and graphics drawing is unsupported.

## Required before real integration

Extract explicit initialization/failure cleanup from the legacy CLI without
implicit extraction, records, dump paths or a second host. Convert remaining
fatal/trap exits and audit the entire lifted stack as C-only. Prove native audio
and RAM publication: pause/neutral/audio/output cancellation still need their
real backend hooks; the current pause test proves the producer gate only.
Keep texture/point data immutable for the session and mutable banks lease-owned.

Full patched engine C compilation/linking, native register application, sound and
output event translation, owner-thread GL preparation, resource fences, texture/
fog/gamma/sprite/HUD composition and calibration are unverified. No lifted game
source or content was loaded in these tests. Implement frame-local camera IDs and
classified triangles from captured records; direct quads must not become targets.
Before rendering, align the runtime's stage-to-scene gun ray with its anchored
eye matrices; the lead owns that shared transform. Physical scale and native
screen calibration remain pending; source hashes and CPU tests cannot accept them.
