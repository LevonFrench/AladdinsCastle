# Snapshot and numeric oracle freeze v1

Proposed source-only contract. Common composition encoding remains the closed
178-byte v1 definition in [PAIRED-EVIDENCE-CONTRACT.md](PAIRED-EVIDENCE-CONTRACT.md).
This document fixes the separate snapshot serialization and oracle bytes before
producer/Viewer source work. CPU vectors are synthetic schema/analytic fixtures,
not captured engine frames. No native source, build, GL gate or runtime changes.

## Snapshot preimage

`scene_snapshot_sha256 = SHA256(preimage)`. Integers are fixed-width little-endian;
signed values use two's complement. Floats are finite IEEE binary32/binary64 LE
as specified, preserving signed zero. No struct padding, JSON text, pointer,
cached GL handle or host integer width. Order below is normative.

| Prefix offset / bytes | Value |
| --- | --- |
| 0 / 18 | ASCII `ACN22-SNAPSHOT-V1` plus exactly one NUL. |
| 18 / 8 | Actual `native_frame_counter`, unsigned64. |
| 26 / 8 | Original camera `raster_width`, `raster_height`, two unsigned32 (640,480 here). |
| 34 / 32 | Original camera `fx,fy,cx,cy`, four binary64 (400,400,320,240 in baseline). |
| 66 / 3 | Background R,G,B bytes. |
| 69 / 15 | Fifteen policy boolean bytes in order below, each 00 or 01. |
| 84 / 4 | `g_tex_fixedcap`, unsigned32. |
| 88 / 1 | `fade_policy`, 00 only: identity/no fade. |
| 89 / 8 | `g_scene_x0,g_scene_x1`, binary32 (0,640). |
| 97 / 1 | Resource count, exactly 03. |
| 98 / 123 | Three 41-byte resource descriptors in ascending ID order. |
| 221 / 4 | Quad count unsigned32, 1..1024. |
| 225 / variable | Ordered quad records, **641 bytes each**, no per-record padding. |

Policy booleans: `shade,fog,fog_before_shade,flat_white,write_prio_alpha,
texel_centre,wide_backdrop,depth_test,g_tex_opaque,g_eng_tex_tile16,g_tex_clipbox,
g_eng_degen_uv_legacy,g_tex_pow2sample,g_tex_pow2alloc,atlas_active`.
Source admission requires shade1, fog0/1, texel_centre1, opaque1; unsupported
flags remain0. Atlas active is recorded; the `g_tex_fixedcap` switch is1 (not a
requested pixel size), retaining the pinned source's fixed maximum for these
small UV ranges. Disable deferred/placeholder texture budgets; unknown callbacks,
nonidentity fade, scene windows, other layers or rendering policies reject.
These requirements describe future driver initialization, not existing defaults.

Each resource descriptor is `id:uint8, byte_count:uint64, SHA256:32 raw bytes`.
ID1 is the resolved 128x256 palette in group/pen/RGB order, 98304 bytes; ID2 is
the full formula texture storage, 16777216 bytes; ID3 is the full raw tilemap
including its packed attributes, 2621440 bytes. Hash **actual complete bytes**,
including zero/unused regions; file names or generator descriptions are not
resource hashes. Full blobs stay private; only synthetic test recipes are public.
The pair requires `snapshot_artifact` (relative private binary file containing
exactly this preimage, no wrapper) and `resource_artifacts` (ID, path, byte count
and SHA for the three complete blobs). Present fog tables have equivalent
quad-index/digest/path references. These references are outside the preimage;
verify their bytes against its descriptors, never hash a path instead of content.
One immutable blob may be referenced by several pairs in the same fresh attempt.
The preimage is a content-addressed root, not permission to omit resource checks.
Immutable retained buffers must match these digests through both eye draws.

Quad metadata, offsets relative to its start:

| Offset / bytes | Values in order |
| --- | --- |
| 0 / 8 | `nrv,ndv`, signed32 each. |
| 8 / 4 | `color`, unsigned32 palette selector, not RGB. |
| 12 / 40 | `texbank,cmode,cz_type,zsort,cz_adjust,flags_raw,cz_value,behind,order,pick_code`, signed32 each. |
| 52 / 16 | `clip[0..3]`, signed32. |
| 68 / 8 | `uvbox[0..3]`, unsigned16. |
| 76 / 8 | `objectflags,direct`, signed32. |
| 84 / 256 | `v[0..3]`, four 64-byte vertex records. |
| 340 / 256 | `rv[0..3]`, same vertex format. |
| 596 / 45 | Resolved per-quad fog record, below. |

Vertex format: `sx16,sy16,z` signed32; `u,v` unsigned32; `bri,valid,uf,vf,bf`
signed32; then `acn_view[0..2]` binary64. This is exactly 40+24 bytes. Captured
`acn_view` retains **board +Z**; canonical negation happens once at staging,
not in this hash. Preserve both native raw and ordinary draw vertices, original
UF/VF/BF, painter key and queue order. Do not reorder or replace by clipped eye
packets. Require nrv4, ndv0, valid1, direct/behind/objectflags0 and complete native
clip window, with the existing stage's source admission. Unused rv/dv slots are
excluded because this admitted path never consumes them; the source still stays
immutable. Any route that consumes those slots needs a new schema.

Fog record: `applies:uint8, table_present:uint8, rgb[3]:uint8, sdelta:signed32,
alpha_const:signed32, table_sha256:32 raw bytes`. Booleans are00/01. Present table
hashes exactly8192 unmodified native lookup bytes; delta remains separate. Absent
table uses all-zero hash bytes, not SHA256(empty). Constant range[-1,255], delta
[-256,255]. If applies0 require absent table, rgb0, delta0, constant255. Every
resolved result comes from immutable fixture material inputs before either eye;
no callback address is serialized. Replacing GPU LUT storage does not mutate
those inputs. Palette/texture/CZ changes require a new acquired snapshot.

The root excludes leases, display/pose/target IDs, caches, timers and geometry
already consumed to produce the queue (point packets, raw mixer registers).
It binds the **resolved native draw inputs**; no post gamma/text/sprite/mixer
pass is admitted. Source implementation must verify the proposed driver policy
and callback resolution actually cover every value consumed by the native draw.

[snapshot-vectors.json](snapshot-vectors.json) contains three complete synthetic
input records and fixed preimages/hashes: no fog, nonlinear table, and one changed
original XYZ value. These test encoding/material/geometry binding; their zero
texture/palette recipes do not pretend to be usable stereo images.

## Oracle byte definition and frozen scene

[oracle-manifest.json](oracle-manifest.json) is the bounded numeric scene authority.
`oracle_manifest_sha256` hashes ASCII `ACN22-ORACLE-V1` plus NUL followed by
canonical JSON: ASCII strings only, unique keys recursively sorted by ASCII
codepoint, compact comma/colon separators, arrays in declared order, lowercase
true/false/null, decimal integer numbers with no leading zeros; no whitespace,
newline or floating-point JSON numbers. Integers are within signed53-bit range.
Nonintegral inputs/expectations use 8-hex-digit binary32 or 16-hex-digit binary64
bit strings as labeled. String escape rules are JSON's quote/backslash escapes;
control/non-ASCII characters reject. Canonicalization is not file hashing.
Producer export uses canonical JSON followed by one LF, never CRLF; its file
digest is separate and hashes the actual exported bytes. A repository checkout's
newline conversion does not enter the oracle hash. No self-referential digest
is included in the manifest.

Oracle coverage is explicitly bounded: exact-source orientation/near-far marker
quads, two display translations on one snapshot, and the rotated/Z-translated
near-clipped quad derived from the existing CPU regression. Formula marker quads
use the existing quad primitive and native texture/material path, not an alternate
rasterizer. Their listed coordinates are **post-preparation original XYZ**;
producer source must produce/compare those exact fields or reject, never tune
the manifest to captured pixels. Native six-plane painter/legacy/LUT-transition
checks in the broader plan remain required source witnesses, not numeric coverage
silently inferred from these vectors. Extend the reviewed oracle before running
an additional case; do not label an uncovered case accepted.

The existing palette-loaded flag must be1, all source buffers present, and no
unrecorded environment/material policy may affect draw. These are source admission
preconditions; hashing the arrays alone cannot establish that native code used them.

Numeric values and tolerances are frozen in the manifest, including both anatomical
eye rectangles and both clipped sample pixel centers. Cameras stay parallel.
Matrices use the closed composition recipe with strict binary32 products/sums,
column k=0..3 accumulation, round each multiply and add nearest/ties-even, no FMA.
Projection matrices are recorded as exact bits, not rebuilt from rounded FOV text.
The independent analytic checker uses binary64 arithmetic on those supplied bits.

Readable marker expectations below are rounded for display; manifest binary64
words are authoritative. Coordinates are eye-local top-left projected centers.

| Pose | Feature | Left (x,y) | Right (x,y) | Disparity |
| --- | --- | --- | --- | --- |
| 0 | Near upper-left | (250.208333,200.937501) | (233.541667,200.937501) | 16.666667 |
| 0 | Far lower-right | (381.718750,264.414062) | (375.468750,264.414062) | 6.250000 |
| 1 | Near upper-left | (255.416666,200.937501) | (238.750000,200.937501) | 16.666667 |
| 1 | Far lower-right | (383.671875,264.414062) | (377.421875,264.414062) | 6.250000 |

Orientation marker centroids: expected projected original centers, pixel tolerance
1.0; disparity tolerance2.0; nearer-minus-farther disparity must exceed8.0 pixels.
RGB classification tolerance2 bytes/channel; each expected marker region must
contain at least64 qualifying interior pixels. Temporal gate requires at least32
changed geometry pixels per marker region in **each** eye across the two poses;
layout/lease/resource digests unchanged. Reserve/exclude a16-pixel image border
for future sequence markers. Marker identities and world geometry diagnostics
are separate; border changes cannot satisfy this gate.

CZ samples: local integer pixel coordinate means center `(x+0.5,y+0.5)` in the
exported top-left image; use the full image X plus640 for the right eye. Check
ray intersection with original triangle, barycentrics and GL near/far admission,
then original board depth lookup. Expected board depth tolerance0.25 board units;
eye depth tolerance0.000002 m; UV tolerance0.004 texel. These are CPU transport
tolerances, not a color acceptance substitute. Dedicated CZ material has constant
brightness64, uniform pen2 RGB(80,100,120), fogRGB(10,20,30), delta0, native table
entry4=130, entry5=250, all other entries0. Both selected centers give alpha125
and expected RGB(44,59,74), tolerance2, safely away from CZ/triangle boundaries.
Eye-depth-as-board lookup gives255 and affine board-Z gives5. Interpolating the
original vertex lookups gives255. Graded brightness/atlas-UV transport retains its
existing separate CPU checks; it is not claimed by this constant-color witness.

| Eye / local pixel center | Original board Z | Eye depth m | CZ index / alpha |
| --- | --- | --- | --- |
| Left (360.5,260.5) | 1064.254366803070 | 0.449875052118 | 4 / 125 |
| Right (330.5,240.5) | 1192.100240995793 | 0.584596109953 | 4 / 125 |

No GPU measurements justify these tolerances yet. They are pre-run thresholds
based on pixel-centroid quantization, binary32 rounding and 8-bit color rounding;
any failed proof requires diagnosis, not post-hoc tolerance expansion. Freeze
additional numeric painter/LUT/endpoint cases before executing them.

## CPU fixture and remaining gates

[check_scene_vectors.py](check_scene_vectors.py) rebuilds snapshot and oracle bytes,
checks fixed digests, independently calculates projected marker centers and CZ
samples from original XYZ, and rejects malformed/mutated resource/metadata cases.
It imports no native renderer, starts no application and creates no images.
Frozen expected bytes are authored separately; private .NET SHA checks provide a
second digest implementation. Agreement establishes serialization/analytic
fixtures only. It does not establish native buffer capture, GL sampling, LUT
upload/state, compositor mapping or Viewer validator behavior.

Lead independent closure of this snapshot/oracle freeze precedes producer source
and Viewer CPU implementation. Source review must verify actual buffer coverage,
per-eye resource preflight and exact scenario adherence before any coordinated run.
