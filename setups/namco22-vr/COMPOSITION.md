# Owned composition plan and CPU oracle

This setup-private contract extends `SceneInput` → `prepare` → `Frame` → the
existing staged immutable lease. `compose_cpu_frame` consumes the leased plan
and owned, explicitly eye-bound decoded RGB/alpha spans. It is intended for a
later board adapter; it does not decode native lists, rasterize game polygons,
anchor sprites/text, infer completeness or enable the native factory.

## Source policy and colour stages

Selected generic engine pin: `a9f1a0a1f9de62387a7138006fa36933fd477123`.
The rationale and source locations are in
[proposal14](../../docs/status/stereo/2026-10-10-14.md): `engine/ss22_gl.c:510-533`,
`engine/quad_gl.c:216-227`, `engine/sprite_hw.c:24-29,169-177,308-322`,
`engine/text_hw.c:99-101,151-155`, `engine/fog_hw.c:172-194` and
`engine/post_gl.c:133-180`. These establish a selected source order, not hardware
or driver pixel parity.

`copy_super22_composition_mixer` reads only an existing owned `VideoSnapshot`:
exact 1024-byte mixer, positive tick, background8..A, fadeRGB16..18, factor19,
flags1A and three 256-byte LUTs100/200/300 (offsets hexadecimal). The copy is
independent of native globals and remains valid after producer mutation.

For `Super22SourceOrder`, all three classes must be declared `Empty` or
`Complete`; `Missing` is unsupported. Empty requires no items; Complete requires
items. Every item carries a unique ID, matching tick and exact colour-stage tag:

| Item | Required input stage | Order information |
| --- | --- | --- |
| Polygon | Post shade/fog/polygon input fade, byte RGB, alpha0/255 | Native 24-bit priority and explicit native emission order; equal-priority polygons reverse emission |
| Sprite | Post fog, before sprite own fade, resolved source alpha | Native 24-bit priority; equal-priority sprites require actual already-sorted order, ascending |
| Text | Before text own fade, resolved ordinary source alpha | One aggregate decoded text layer; no native priority |

Completeness and stage labels are caller assertions, not evidence that a native
display list was fully decoded. Unknown tags/modes, ambiguous sprite ties,
duplicate polygon tie keys and multiple independently ordered text items reject.
Spot/shade, moving HUD, native sprite anchoring and unsupported native modes must
be resolved in an approved adapter contract before they can be labelled supported.
Do not flatten polygons into one world image: sprites can interleave between them.

## Composition and quantization

For channel bytes C/F, factor f and source alpha a, the scalar policy is:

1. Start with mixer background. Merge polygons and sprites in descending native
   priority; sprites go first at polygon ties. Sprite own fade applies when
   f>0 and (flags bit1 or item fade): `floor((C*(255-f)+F*(f+1))/256)`.
   Then source-alpha blend: `floor((src*a+dst*(255-a))/255)`.
   Any nonzero-alpha prioverchar sprite marks coverage, even if a later polygon
   covers its colour; transparent sprite pixels do not mark it.
2. Global screen fade, flags bit0 and f>0, uses the same `/256` expression over
   background/polygons/sprites. Zero factor bypasses the gate exactly.
3. Text own fade, flags bit1 and f>0, uses
   `floor((C*(255-f)+F*f)/255)`. Masked text pixels contribute no alpha; blend
   other text over the already screen-faded image.
4. Apply byte-indexed RGB LUTs last. Any nonzero LUT byte enables all channels;
   all-zero LUTs bypass gamma as uninitialized **in this source pin**.

Byte-floor checkpoints at framebuffer blends are an explicit **software oracle
policy**. Actual GL_UNORM conversion/rounding remains unverified. Native integer
sprite/text own-fade expressions are kept separate from those blend checkpoints.
The existing polygon shade/fog/input-fade oracle still retains fractional shade
and raw fog through its final byte conversion: the independent318/255→1 witness
would become254/255→0 if fog were prematurely floored.

Literal composition witnesses include world100→screen60→L60=7, opaque text200→
L200=19 (not faded110→23), text alpha128 over60→130→L130=29 (not gamma-first13),
white world/sprite factor1→253 versus text254, and sprite own-plus-screen→251.
Tests cover fade flags0/1/2/3, item flags, masks, ties and zero/LUT gates.

## Eye binding, budgets and transactional output

The payload owns an `acvr_eye` and one record for every plan ID; an item's empty
span vector explicitly means no visible pixels. Frame ID, eye index, layer,
rectangle and both matrix arrays must match the current requested eye exactly
(matrix bits, not a tolerance). A new pose needs newly produced/bound spans;
merely re-labelling old native pixels does not establish correct reprojection.

Spans are top-left eye-local rectangles with owned RGB/alpha vectors. Rectangles
within one item must be disjoint; between items overlap is expected. Dimensions,
sample counts, identifiers, declarations, stages, tick, ties and cumulative
capacity are checked before any output write. Provisional admission limits are
4096 items, 65536 cumulative spans, eye edges4096, 4Mi eye pixels, 64MiB cumulative
owned input storage and 64MiB private working storage. Vector capacities count
alongside plan/item/span metadata. These are implementation limits, not claims
about complete native frame capacity; oversized inputs reject rather than truncate.

The compositor calculates into private RGB/mask storage, then commits only the
eye's rectangle in `Image`. Lower-left eye rects map to top-left image rows.
It does not resize the caller image or read/write physical depth values; it
validates the existing depth plane size. Gun/shared depth, other eyes and outside
pixels remain unchanged. Native priority never substitutes for physical depth.

Absent plans preserve existing CPU/GL polygon-only draw behavior. Present plans
reject in those draw paths (including HUD-only CPU) before output mutation or any
GL command/upload/clear: only this CPU entry can fulfill them today. Existing
lease sequencing, producer ownership, paired-eye replay, new-pose rejection and
successor ticks are covered through the same backend callbacks.

## Remaining acceptance

No GL finalization, scratch/LUT textures, new graphics dispatch, ABI changes,
native admission, real game geometry, actual GL/context/window/GPU, engine entry,
game/generated-source/BIOS access, install/download or headset/Steam writes.
Native decoding/coverage/order, camera/cull expansion, per-game scale, sprite and
text placement, true physical depth, runtime guns and actual driver/game/headset
acceptance remain separate. Board1 is unfinished; other boards await lead direction.
