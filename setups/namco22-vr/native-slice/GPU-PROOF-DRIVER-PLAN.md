# Native batch proof driver: bounded source plan

Status: proposed, unimplemented, no build or GPU execution. First review the
[producer contract](PAIRED-EVIDENCE-CONTRACT.md), then assign producer source and
Viewer CPU validation. PR60 exact `074f771` supplies the guarded native batch;
its review is a dependency. This document does not authorize a run or change its
hardcoded OFF draw/noncapture gates, `begin_eye`, backend capability or Viewer.

## Source and gate design

Extend the existing formula fixture and own `fixture.c`, `eye.c`, `pose_stage.c`,
`native_batch.c`, guarded `native_patch.py`, preparation and CMake files only
after a source assignment. Keep the twelve approved generic native units, pinned
input hashes and cached SDL SDK. No new upstream unit, third-party dependency,
loader, alternate rasterizer or fake GL handles. Retain the old executable/check
path and accepted PR53 artifacts unchanged; do not repurpose its `--draw` result
as a native-batch or receipt proof.

Proposed separate executable `n22_native_batch_proof` uses those same units with
an explicit proof-only build gate; the default `n22_native` library keeps gate 0.
The option is absent today and must be reviewed before implementation. Runtime
invocation additionally requires the reviewed scenario, fresh private destination
and coordinated GPU slot. Compile success alone never enables public graphics.
Its command rejects default/unknown scenarios before SDL video initialization.
Public ABI remains unchanged; evidence is a setup-local sidecar, not pointer-bearing
shared memory or an XR provider. Production backend graphics stays unsupported.

Source admission before either eye: immutable queue digest and bounded packet
capacities; distinct input/output storage; both eyes staged; supported ordinary
opaque textured materials; original painter order; all native bake/table/storage
resources ready; 3.3+ compatibility context and required entry points; native
640x480 ortho/identity modelview; no GLSL/VBO/VAO, fixed fog/light/cull or depth
test; unit1 baseline, texture-size limit and default pixel-transfer scale/bias.
Existing source has no whole-pair resource-preflight API. Design/review that
before calling the proof path; do not claim current per-fragment checks supply it.
Any late failure invalidates the whole pair and prevents swap/acceptance receipt.

## Synthetic cases and independent pixel oracles

| Case | Reused input and independent expectation |
| --- | --- |
| Baseline/native painter | Existing six colored checker planes, forced-far near red and stable equal-key green/magenta witnesses. Native queue remains groups 5,0,2,4,3,1; check both eyes after all clears. Homogeneous new path is compared with the old path in separately identified artifacts, never mixed into one purported pair. |
| Order/orientation/depth | Formula geometry includes unequal left/right and top/bottom features; distinct near/far features have greater positive local left-minus-right disparity when nearer. Parallel eye rotations, ±0.032 m eye offsets (±32 board-unit equivalent) at provisional 1000 units/metre, asymmetric off-axis terms; no toe-in. Independent analytic positions use the ORIGINAL XYZ and supplied matrices, not captured packet output. Reject swapped eyes, X/Y flips and duplicated mono output. |
| Rotated near clipping and CZ | Reuse the CPU regression's original Z768/1536 quad and nonlinear `(i*53+19)%256` alpha table. Its yaw0.18, view X200/Z600 and near160/far8192 board-unit regression is reproduced in the driver's metre-valued composition with X0.2/Z0.6 m and near0.16/far8.192 m. Screen-pixel-center barycentrics come from independent original triangle projection/clip calculation. Interpolate ORIGINAL board Z through eye W before table lookup; distinguish eye-depth fog (converted back to board units), affine board-Z fog and interpolated vertex lookup. Existing CPU lambda sample gives CZ4/alpha125 but is not automatically a GPU pixel center. Choose interior centers safely away from triangle/CZ boundaries and publish their expected indices before the run. |
| LUT replacement and native return | Consecutive native batches use two different CZ tables/fog colors, then a legacy textured draw. Earlier queued pixels must retain the first table; later pixels use the second. Verify unit1 matrix stack/content, client arrays, vertex pointer size, active/client unit, unpack state and legacy witness after explicit flush/end. Scope only the declared native baseline, not arbitrary external state restoration. |
| Step endpoints | Dedicated flat-depth patches at representable Z just below/on/above256 and near clamped table ends, plus approved constant/delta cases. Record actual indices/pixel ranges and float-rounding limits. Do not demand unproven hardware byte parity at arbitrary interpolated step boundaries. |
| Pose replay | Two deterministic display poses share one captured scene lease/native counter/digest; change common yaw/translation while maintaining parallel per-eye cameras. Both interiors' known world features change, matrices/revisions/display sequences advance, scene does not step. Repeat one identical pose for deterministic pixel/metadata replay as a separate record. |

Each case pins its composition order and formula inputs in the oracle manifest.
The table lists required witnesses, not completed numeric pixel expectations;
final source review must establish the actual pixel centers/tolerances before a
GPU slot. Separate scene cases release/acquire snapshots between cases. LUT
replacement selects immutable per-quad tables, not mid-lease fog-source mutation.

Publish oracle data/tolerances with reviewed source, using a small color-channel
tolerance justified by framebuffer precision/dithering and excluding edge pixels.
Disable multisampling and dithering for this proof; retain native opaque texture,
palette and shade/fade behavior. Record actual color bits and GL configuration.
Per-eye diagnostics exclude any sequence/orientation border markers. Semantic
checks use geometry features, not marker changes or whole-window RGB variation.
No claim of gamma/text/sprite/HUD, widened game culling, gun-depth occlusion or
game parity. Clip Z transport can be checked in CPU but shared depth stays deferred.

## Driver lifecycle and private evidence

One owned nonresizable 1280x480 drawable, compatibility GL context, process and
owner thread. Verify actual drawable dimensions and scissor every eye clear/draw.
Use existing native batch/pass controls and flush before any LUT replacement,
legacy transition, eye boundary or readback. Do not step/render native simulation
per eye or resort fragments. Fresh private directory/artifacts are reserved before
GPU work; preallocate bounded readback/receipt storage and reject resource failure.

For each case, stage/preflight both eye packets, draw them into one back buffer,
flush, check every GL error, finish once, read GL_BACK once, reverse export rows
once, check independent eye features and compute full/region digests. The receipt
binds exact matrices, common composition revisions, frame lease and target/pair
sequence under [v1](PAIRED-EVIDENCE-CONTRACT.md). Save the untouched PPM; optional
PNG is lossless and must have identical decoded RGB bytes. Hash files after close.
Only a checked complete pair is eligible for one swap attempt. SDL swap returns
no compositor proof; WGC/Viewer admission and handshake remain separately assigned.

Proposed run bounds: at most 16 pair records, 15 seconds total from process start,
at most 3 seconds presenting any one case, no automatic retries, no OS input,
system changes, game files, OpenXR or headset. Exit on SDL_QUIT or bounds and close
owned context/window/process normally; verify PID absent. If shutdown does not
complete, lead-owned cleanup is required and result is failed, never acceptance.
One diagnosis and one separately coordinated retry at most. Retain failed receipts.

## Review and handoff sequence

1. Close ARC-PR61-01: lead independently verifies the frozen 178-byte common
   composition encoding and four hash vectors in the producer contract. Only
   after closure assign producer or Viewer implementation against that exact
   contract hash; Dolphin live admission and schema-1 receipts stay untouched.
2. Assign producer/driver source; review isolated proof gate and resource preflight.
   CPU checks cover lifecycle/serialization/correlation adversaries without SDL
   video/GL. A scheduled native compile uses only approved cached inputs, no run.
3. Freeze the snapshot field serialization, independent byte/hash vectors and
   fixture oracle/tolerances in source review BEFORE any producer run. Lead then
   coordinates an explicitly authorized bounded GPU slot for the exact
   executable/hash and scenario. Common-composition vectors do not freeze the
   separate snapshot serialization or prove scene/pose correctness.
4. Record producer paired candidate and synthetic stereo separately. Only a later
   specific Viewer source/run assignment can establish compositor delivery.
5. Lead decides whether native GPU proof is accepted and what source integration
   follows. Do not turn on `begin_eye` or public capability automatically.

At this checkpoint only the plan and CPU composition hash fixture exist. No
producer executable, image, receipt serializer, Viewer validator, new runtime
permission or measured rendering result.
