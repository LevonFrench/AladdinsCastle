# System22 paired producer evidence contract v1

Status: proposed, source-only, unimplemented. This contract specifies the next
synthetic proof driver and a later Viewer CPU validator. It does not upgrade old
images or Viewer schema-1 receipts. GPU/noncapture gates remain OFF. PR60
`074f7715206cf239992004644a1089d7e89c37e4` is the explicit source dependency;
its source review and integration are separate from this contract review.

The current `fixture.c` prepares once, draws two 640x480 eyes, reads the complete
1280x480 GL back buffer, flips rows into PPM order, then swaps once. It has no
paired receipt or compositor correlation. ABI inputs are `acvr_eye` and
`acvr_draw_info` in [acvr.h](../../../libacvr/include/acvr.h); frame lifetime
follows [renderer-interface.md](../../../docs/renderer-interface.md). The
standalone fixture does not currently acquire an `acvr_frame` lease.

## Admission and scope

The future producer kind is `system22-native-synthetic`, scenario
`native-batch-paired-v1`, capture kind `producer-gl-backbuffer`. The launcher
must pin the reviewed source/overlay, built executable and SDK hashes, and own
the process/window. It selects that exact process and one client window, never
an arbitrary desktop/title match. Expected future basename is
`n22_native_batch_proof.exe`; no such executable is claimed to exist today.
Reject ambiguity, wrong identity, resize, stale/minimized window and wrong
drawable size. Only the existing approved native units and formula fixture
inputs are admitted. All artifacts and process/path details stay in ignored
private storage. No game, loader, OpenXR or headset is part of this scenario.

Viewer's current Dolphin-only admission stays unchanged. A future approved
System22 host needs a separately assigned admission implementation; this contract
alone cannot admit it. The producer-readback and WGC capture kinds remain distinct.

## Receipt envelope and exact pixels

Use UTF-8 JSON, `schema: "acn22-paired-proof/v1"`, with a bounded array of complete
pair records. A future validator rejects duplicate keys, unknown schema/capture
kind, missing required fields, nonfinite numbers, malformed hashes and excessive
lengths. Unknown fields require a schema revision. IDs and nanosecond counters
are unsigned/signed decimal strings respectively, never JSON floating-point IDs.
SHA-256 digests are 64 lowercase hexadecimal characters. Proposed limits are
16 pairs, 1 MiB JSON, and 1280x480 RGB8 images only for this scenario.

| Required run fields | Meaning |
| --- | --- |
| `run_id`, `producer_kind`, `scenario_id`, `capture_kind` | Fresh attempt identity; explicit source and evidence scope. |
| `source_commit`, `upstream_commit`, `overlay_sha256`, `exe_sha256`, `sdk_manifest_sha256`, `contract_sha256`, `build_receipt_sha256`, `private_launch_receipt_sha256`, `oracle_manifest_sha256` | Exact reviewed inputs and owned-process admission; upstream is the existing pinned native revision. Oracle manifest pins formula inputs, expected features and tolerances. Do not infer binary identity from a branch name. |
| `attempt`, `exit_code`, `owned_processes_absent`, `result` | One attempt, diagnosed retry at most once; `complete`, `failed` or `aborted`. Only complete records can be acceptance candidates. |
| `pairs` | Each immutable pair JSON is referenced by relative private path and SHA-256. A final run manifest is written only after normal cleanup; failure diagnostics are explicitly incomplete. |

`run_id` is a freshly generated 128-bit lowercase hex token, not a window title;
`attempt` is integer 1 or 2, `exit_code` is a signed integer, and
`owned_processes_absent` is boolean. Identity/hash validation is separate from
trusting `result`; a self-reported success cannot replace independent checks.

Retain the untouched full-client/full-target image, not just cropped eyes. Each
pair names a fresh relative private artifact, `file_sha256`, dimensions and
`full_rgb_sha256`. File digest hashes all PPM/PNG bytes; RGB digest hashes decoded
RGB8 bytes in top-to-bottom row-major order, no stride/padding/metadata, R,G,B per
pixel. Opaque alpha, if a later WGC PNG has it, must be checked then discarded;
other formats/conversions are unsupported in v1. Private paths must remain inside
the attempt directory, with no traversal, symlinks/reparse traversal or overwrite.

| Pair layout fields | Required value |
| --- | --- |
| `width`, `height`, `pixel_format`, `row_origin`, `layout` | 1280, 480, `RGB8`, `top-left`, `sbs-left-right`. |
| `presentation_transform`, `mirrored_x`, `mirrored_y` | `identity`, false, false. GL readback row reversal is recorded as an export conversion, not an image-space presentation mirror. |
| `eyes[0..1].eye_index`, `array_layer`, `image_rect` | Left 0 / right 1; layer 0; `[0,0,640,480]` / `[640,0,640,480]` in exported image coordinates. |
| `eyes[].draw_rect`, `draw_rect_origin` | Same numerical rectangles in this full-height case; `lower-left` for GL. General conversion is imageY = height - drawY - drawHeight, not silently assumed equal. |
| `eyes[].rgb_sha256` | SHA-256 of each exact rectangle's decoded RGB8 bytes in top-left order. Independent hashes of the full image regions, not hashes of re-encoded crop files. |

Rectangles must be integral, positive, in bounds, nonoverlapping and cover the
declared SBS target exactly. Eye order is anatomical left/right, not draw order
inferred from unequal pixels. Asymmetric synthetic features independently verify
order and +X/+Y orientation. Supplementary crops never replace the original.

## Scene, pose, projection and pair correlation

Each pair carries `case_id`, `run_id`, `scene_frame_id`, `lease_id`, `lease_generation`,
`scene_snapshot_sha256`, `native_frame_counter`, `display_id`, `pair_sequence`,
`pose_revision`, `anchor_revision`, `scale_revision`, `composition_sha256`,
`target_generation`, `target_width/height` and two complete eye inputs.

Standalone mode labels `lease_kind: "fixture-immutable-snapshot"`: mint the
lease ID on actual snapshot acquisition, bind its lifetime to the immutable
prepared queue, and invalidate it on release. It must not fabricate a runtime
lease. A future ABI host uses `lease_kind: "acvr-frame"` and copies real
`acvr_draw_info.frame_id/display_id`; it may not upgrade standalone IDs later.
Snapshot digest serializes defined source geometry/material/camera fields and
painter order, including immutable palette, texture and per-quad CZ/fog inputs,
excluding pointers, C padding and mutable GL handles. Updating GL texture storage
from those immutable tables does not mutate the source. The implementation must
publish that byte serialization for review. New cases may acquire a new snapshot
only after releasing the prior lease; replay cases keep one snapshot unchanged.

One pair requires the same live lease, source digest, native frame counter,
display ID, pose/anchor/scale revisions and target generation in both draw
records. Each eye records its own exact view and off-axis projection, clip
convention `desktop-gl-minus-one-to-one`, and provisional scene scale. Composed
views are right-handed +X right, +Y up, -Z forward. Record common head transform,
anchor transform, scene scale and per-eye eye offset separately as composition
inputs; verify they produce the recorded views. No second native projection.

| Correlation field types | Normative representation |
| --- | --- |
| `scene_frame_id`, `lease_id`, `lease_generation`, `native_frame_counter`, `display_id`, `pair_sequence`, `pose_revision`, `anchor_revision`, `scale_revision`, `target_generation` | Decimal strings representing actual nonnegative counters/IDs in the declared producer domain. Pair sequence strictly increases within a run. Standalone display ID is explicitly a producer display counter, not an XR display ID. |
| `head_from_anchor`, `anchor_from_scene`, `eyes[].eye_from_head`, `eyes[].view_from_scene`, `eyes[].projection_from_view` | Finite binary32 matrix arrays of length 16. Their matching `*_sha256` fields hash the exact float bytes. Common rigid transforms are in metres after source scale conversion. |
| `scene_units_per_metre`, `scale_provisional` | Positive finite binary32 value and boolean; standalone initial scale 1000 is provisional, not game calibration. |
| `composition_sha256`, `scene_snapshot_sha256`, `oracle_manifest_sha256` | Required SHA-256 digests, identical between both eye draw records for a pair. Common composition excludes eye-specific offsets. |
| `eyes[].draw_correlation` | Copy of scene/lease/display/pose/anchor/scale/target IDs observed on entry and exit of the actual eye emission. Both records must agree with the pair; a missing callback record cannot be reconstructed afterward. |
| `timestamps` | Domain-tagged objects with decimal counter/frequency or nanoseconds. `predicted_display_time_ns` is null in standalone mode, real runtime monotonic time in ABI mode. No WGC timestamp in a producer-readback record. |

Compose `view_from_scene = eye_from_head * head_from_anchor * anchor_from_scene * S`,
where S is diagonal `(1/scene_units_per_metre, 1/scene_units_per_metre,
1/scene_units_per_metre, 1)`. Source snapshot XYZ is already canonical -Z after
the one board +Z conversion. Composition digest covers those common inputs and
scale; per-eye view digest also includes its eye offset. Source review specifies
float rounding and validates the actual supplied matrix against this recipe.
The standalone metre-valued view/near/far may reproduce an old board-unit test's
screen pixels, but those matrices have different bytes and must be recorded as used.

### Frozen common-composition preimage

ARC-PR61-01: `composition_sha256` is SHA-256 of the following **178 bytes**,
concatenated in this order. No JSON text, field names, lengths, separators,
padding or trailing newline are hashed. This defines the common composition
only; it is not the scene snapshot or an eye/presentation digest.

| Offset / bytes | Receipt input | Encoding |
| --- | --- | --- |
| 0 / 21 | Fixed domain tag | ASCII `ACN22-COMPOSITION-V1` followed by exactly one NUL byte; hex `41434e32322d434f4d504f534954494f4e2d563100`. |
| 21 / 64 | `head_from_anchor` | Elements 0 through 15, column-major, each IEEE-754 binary32 little-endian. |
| 85 / 64 | `anchor_from_scene` | Same matrix encoding. |
| 149 / 4 | `scene_units_per_metre` | Positive finite IEEE-754 binary32 little-endian. Hash the scale input, not the derived reciprocal. |
| 153 / 1 | `scale_provisional` | JSON boolean true -> `01`, false -> `00`; integers/strings are invalid. |
| 154 / 8 | `pose_revision` | Unsigned 64-bit little-endian integer. |
| 162 / 8 | `anchor_revision` | Unsigned 64-bit little-endian integer. |
| 170 / 8 | `scale_revision` | Unsigned 64-bit little-endian integer. |

Revision JSON strings use canonical decimal grammar `0|[1-9][0-9]*`, range
0..18446744073709551615; no sign, whitespace, exponent or leading zero. Floating
inputs are the recorded binary32 values: parse JSON decimal numbers by correctly
rounding to binary32, nearest with ties to even. Preserve negative zero, reject
NaN/infinity/overflow and boolean-as-number. The producer's decimal output must
round-trip to the actual supplied binary32 bits; a receiver may not silently
canonicalize signed zero. Matrix component order is never sorted/transposed.

No other fields enter this digest: run/lease/frame/display/target IDs, timestamps,
eye offsets, eye view/projection, their hashes, clip convention and image bytes
are excluded and independently validated/bound in the pair record. The fixed
domain identifies v1's handedness/composition recipe; any change of recipe,
encoding, field set or domain requires a new schema/domain and new vectors.
Receipt key order/JSON formatting have no effect; do not use a struct-memory hash.

[composition-vectors.json](composition-vectors.json) freezes four receipt-input,
preimage-hex and SHA-256 vectors: baseline, provisional flag toggle, posed input
with signed zero and large revisions, and its positive-zero twin. Expected bytes
were constructed separately from explicit IEEE integer words, not from the
checker's encoder. [check_composition_vectors.py](check_composition_vectors.py)
independently rebuilds them from receipt fields using Python standard-library
packing and checks rejection/mutation cases. This small CPU fixture is not a
producer or Viewer receipt implementation; it does not validate scene physics,
pixel correlation or all general-purpose decimal parser edge cases.

Lead closure of ARC-PR61-01 is required BEFORE producer or Viewer implementation.
Source review must also freeze exact snapshot serialization and oracle manifest
before a producer run; that snapshot format is still unspecified here. A CPU
validator can reject structural inconsistency first, but may claim synthetic
stereo only after independently checking the pinned scenario's pixel features.

Matrix arrays contain 16 finite values, column-major/column-vector order. Their
digest bytes are little-endian IEEE binary32 values actually supplied to draw,
including the sign of zero. Record each matrix digest and a digest of the common
composition inputs. JSON decimal output must round-trip to those exact floats.
Timestamps carry explicit domains: `producer-qpc` has counter/frequency,
`runtime-monotonic-ns` has real predicted display time, and `wgc-system-relative`
belongs only to a WGC capture. Standalone predicted XR time is null. Timestamps
are supporting chronology; cross-domain proximity never establishes correlation.

Lifecycle: acquire snapshot -> stage/preflight BOTH eyes and resources -> draw
both rectangles -> flush/check GL -> read one complete back buffer -> validate
pixels/hash -> mark paired render complete -> one swap attempt -> cleanup/release.
No step, palette/fog/source mutation or lease release occurs between eye draws.
On any failure abandon the target and do not swap/publish a successful pair.
Readback binds pixels directly to that exact target generation and pair sequence
before swap. Record `presentation: "swap-requested"` separately; SDL swap return
and producer readback do not prove compositor delivery or window appearance.

A future WGC pair needs its own original artifact and a verified mapping. Proposed
mapping: producer renders a bounded sequence/eye marker in reserved borders,
publishes a completed-pair handshake, swaps, and holds that pair without advancing
until Viewer captures and decodes matching run/sequence/eye markers and returns
an acknowledgement binding its artifact hash. Producer may time out and abort;
an acknowledgement alone without decoded matching pixels is insufficient.
Markers contain only run/sequence/eye identity, not a digest of the image containing
them. Handshake references the completed immutable producer pair receipt digest;
acknowledgement references that digest and the WGC artifact digest. Final run
manifest references both. This avoids circular image/receipt hashing. Swap outcome
belongs in the final manifest, not an edit to the immutable pre-swap pair receipt.
Viewer must validate client geometry/identity and preserve both clock domains.
Do not hash-match WGC and producer pixels unless RGB bytes actually match under
the declared identity conversion. Any color-management/size transform requires a
new reviewed policy. Until that protocol is implemented and independently checked,
`compositor_correlation: "unverified"`; no invented WGC sequence or runtime IDs.

## Diagnostics and acceptance boundaries

Each eye independently reports interior content and temporal change, stable
layout, analytic feature positions, orientation and near/far parallax. Reserved
markers/borders are excluded from content/change diagnostics. For this scenario,
two completed display poses on the SAME scene lease must change known world
features in BOTH interiors while the native simulation/source digest stays fixed.
Blank right eye or only-left animation fails the paired candidate gate even if
the full window changes. Thresholds and feature tolerances are fixed by the
reviewed fixture oracle before runtime; they are not guessed after seeing pixels.

Record separate results for `paired_capture_candidate`, `synthetic_stereo`,
`compositor_delivery`, `game_stereo` and `headset_acceptance`. v1 direct readback
can support only the first two after their checks pass. Unequal eye hashes alone
prove neither stereo depth nor correct eye order. Static/uniform legitimate scenes
are outside this particular animated acceptance gate, not universally invalid.

Future CPU validator fixtures: valid pair; blank right; only-left change;
swapped/crossed eyes; mirrored/upside-down feature placements; identical-eye
near/far scene; overlap/out-of-bounds/resize; file or region digest mutation;
missing/expired lease; changed frame/pose/projection/target between eyes; stale or
absent presentation sequence; duplicate keys; fabricated runtime time; clock
proximity without mapping; marker-only animation. Preserve old receipt semantics.
All of these are planned cases, not executed results in this checkpoint.
