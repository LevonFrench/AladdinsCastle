# Paired producer / offline Viewer interface v1

Source interface proposal, not runtime evidence. This is the concrete producer
shape for the two frozen scenes; trusted expected source/binary/resource/oracle
hashes are supplied by the lead, never admitted just because a receipt says so.
No Viewer live admission, WGC mapping or additional numeric case is authorized.

## Role and lease order

Exactly four paired records, in this order:

| Pair sequence | `role_id` | Scene / pose | Lease / frame |
| --- | --- | --- | --- |
| 1 | `markers-pose0` | markers / phase0 | fixture lease1, frame1 |
| 2 | `markers-pose1` | markers / phase1 | SAME lease1, frame1 |
| 3 | `markers-replay0` | markers / phase0 | SAME lease1, frame1; RGB identical to pair1 |
| release | `release-markers` | after pair3 swap request, before CZ acquisition | invalidate lease1 packets; native counter remains1 |
| 4 | `cz-clipped` | CZ / phase0 | NEW lease2, frame2 |
| release | `release-cz` | after pair4 swap request, before cleanup | invalidate lease2 packets; native counter remains2 |

These small counters are actual standalone producer counters, not XR IDs. The
four `display_id/pair_sequence/pose_revision` values advance1..4 even for replay.
Lease/snapshot/resource bytes remain unchanged during the first three pairs.
The second scene acquires only after explicit first release. No game step/inputs.

## Exact complete key sets

The machine-readable key lists in [producer-interface.json](producer-interface.json)
are authoritative for complete receipts; reject missing/unknown/duplicate keys.
Failed manifests use the same run keys with null unavailable values, result
`failed/aborted`, and only already validated immutable pair references. They are
never complete acceptance evidence. Pair/eye/stamp/readback/swap/release schemas
are distinct; do not interpret a native intermediate JSON as a completed receipt.
All public IDs/revisions/counters/timestamps are decimal strings, all SHA values
lowercase64hex, matrix arrays finite binary32 values in column-major order.
`evidence_class` is `producer-native-run` for an actual authorized producer run;
the example uses `source-synthetic-example` and must fail live-evidence admission.

Each run `pairs` element is `{artifact,sha256,swap_artifact,swap_sha256}`. Each
`release_events` element is `{artifact,sha256}`. Private artifact paths are fresh,
relative, bounded and reject traversal/reparse points. Snapshot/resource/fog
artifact formats follow [SNAPSHOT-ORACLE-FREEZE.md](SNAPSHOT-ORACLE-FREEZE.md).
Resource entries are `{id,artifact,byte_count,sha256}`; fog entries use
`quad_index` instead of id. Empty fog list is mandatory on marker pairs.

Eye `entry` and `exit` are full identity stamps for the actual native emission:
frame/lease/generation/native-counter/display/pair/pose/anchor/scale/target plus
view/projection/common-composition hashes. Hashes are controller-computed from
the exact native-exported input bits, then bound by the DRAW command; native
entry/exit capture includes the actual input bits and IDs. They must agree with
each other, the staged inputs and pair envelope. This records packet-stage input
consumption, not a call to a production ABI `game_draw_eye`.

`preflight` binds the actual native input-before/input-after, cache-before/cache-
after and device JSON artifacts by separate file SHA values. These private audit
files are diagnostic inputs, not completed paired receipts. Predicted XR display
time is explicitly null in `readback`; only producer QPC is recorded.

`readback` records GL_BACK, source lower-left, export top-left, one row reversal,
actual producer-QPC counter/frequency, paired sequence/target and geometry-complete.
`swap` records `swap-requested` only, actual call QPC, image RGB digest and identity.
SDL's void swap API is not compositor delivery. `release` records the actual
invalidated lease/last sequence/native counter and QPC; it is not inferred from
timestamps. No cross-domain timestamp proximity can establish delivery.

## Original vertex and material roles

Snapshot quads are in native painter order. Marker snapshot index0 is far:
palette group2, original order1, key4096, center(600,-250,4096), half-size(256,128).
Index1 is near: group1, original order0, key1536, center(-300,150,1536), half-size
(64,48). Vertices0..3 are left/top, right/top, right/bottom, left/bottom in board
XYZ; original Z stays positive. UV is (0,0),(63,0),(63,63),(0,63), brightness64,
fog inactive. Palette pen2 RGB is far(240,160,35), near(30,155,245).

CZ snapshot has one quad, group6, order0, key8000, type2. Exact original XYZ and
Q16 UV are the frozen oracle's four rows; brightness64, RGB(80,100,120), active
fogRGB(10,20,30), delta0, constant-1, table[4]=130/table[5]=250/rest0. It is not
the old graded-brightness CPU sample. All scenes use native opaque texture mode,
tilemap zero, tile0's256 pen bytes2, all remaining texture storage zero; palette
zero except pen2 of groups1,2,6. Validate these actual source bytes/metadata.

## Exact pixel predicate rules

Images are untouched full-target P6 RGB8 or raw RGB,1280x480, top-left. Anatomical
left is x0..639, right640..1279. Full/eye SHA definitions are unchanged. At local
(10,10), both clears must be exactly RGB(12,18,28). Exclude a16px border from
geometry content/change diagnostics. No resampling, mirroring, mono duplication
or cropped replacement artifact.

For each marker/eye/pose, decode authoritative expected centroid `(cx,cy)` from
the frozen binary64 words. Search x `[max(16,int(cx)-40),min(624,int(cx)+41))`,
y `[max(16,int(cy)-35),min(464,int(cy)+36))`; `int` truncates toward zero. A pixel
qualifies when all three RGB channel differences from that marker color are<=2.
Count>=64. Measured centroid is mean of ALL qualifying pixel centers (x+0.5,y+0.5),
not bbox center or one chosen sample. Each coordinate error<=1.0. Compute near/far
left-minus-right centroid disparity; each differs from frozen expectation<=2.0,
and near-minus-far>8.0. Swapped/mirrored/blank eyes cannot pass solely via hashes.

Temporal comparison is pair1 vs pair2. Per marker/eye, use union ROI from the two
frozen poses: truncate min/max expected center coordinates, extend x-40/+41 and
y-35/+36, clamp to the same16px interior. Count pixels whose exact RGB bytes
differ AND whose before OR after RGB qualifies for that marker (<=2/channel).
Require>=32 for EACH marker in EACH eye. Whole-image change, background/marker
border motion and only-left changes cannot satisfy this. Pair3 RGB is byte-exact
replay of pair1, with newer display/pose revision but the same snapshot lease.

CZ independently checks both frozen local pixel centers (left360,260/right330,240)
against RGB(44,59,74), <=2/channel, and requires>=64 matching pixels in the bounded
ROI x[sampleX-12,sampleX+13), y[sampleY-20,sampleY+21). This extra interior-content
check is stricter than isolated sample agreement; it adds no new numeric CZ case.
It is a CZ pixel witness, not by itself a generic stereo/comfort detector.
Diagnostics are recomputed by Viewer; producer flags/counts are not proof.

## Source serializer example

[producer-source-example.json](producer-source-example.json) contains the exact
serializer shape for all four roles and two releases, with source-synthetic class,
zero trust hashes, no image artifacts and no accepted candidate flags. It cannot
be upgraded to runtime evidence. It exists to agree keys/types/identity ordering.
No install/build, native process, GPU, game or Viewer run created this example.
