# System 22 material evidence and frame seam

Authority is the generic engine at pinned commit
`a9f1a0a1f9de62387a7138006fa36933fd477123`. No game translations, content,
BIOS, capture data, loader or generator are evidence for this block.
The following are source behavior, not independently verified board accuracy.

## Texture, UV and palette

- [`geo_hw.h`](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/geo_hw.h)
  retains raw, near-clipped and guard-clipped vertices, integer UV/brightness and
  their pre-truncation fraction fields. Quad colour is a palette selector, not
  RGB. Preserve texbank, cmode, objectflags, cz_type/cz_adjust, order/zsort,
  original whole UV box and clip window along with the geometry/camera.
- [`tex_bake.c` lines 36–88, 159–189](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/tex_bake.c#L36)
  masks U to 12 bits, masks V then adds the four-bit bank in bits 12–15.
  Map index is `((v & 0xfff0) << 4) | ((u & 0xff0) >> 4)`; the tile word is
  little-endian at `2*index`. Packed attribute is **high nibble for even index**,
  low for odd. Bit 0 extends tile to 17 bits unless the board uses tile16.
  Flip X (bit 2), flip Y (bit 1), then transpose (bit 3); sample the 16x16 pen
  tile in row order. Code checks the fixed 16 MiB texture limit before fetching.
- Cmode bit 2 selects 2-bit pens: offset `0xec + ((cmode & 8) << 1)`, shift
  `2*(~cmode & 3)`, mask 3. Otherwise bit 1 selects 4-bit pens: offset
  `0xe0 + ((cmode & 8) << 1)`, shift `4*(~cmode & 1)`, mask 15. Otherwise
  offset/shift are zero and mask 255. Select palette group `(colour >> 8)&127`.
- [`eng.c` lines 64–95](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/eng.c#L64)
  contains a board-specific tilemap fixup and explicit tile16 flag. Do not
  infer the flag from game name or apply this transform to unknown content.
  Palette consists of 128 groups of 256 RGB entries from three 0x8000-byte
  planes. Per-group generations invalidate native texture caches.
- [`quad_gl.c` lines 552–584, 665–719](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/quad_gl.c#L552)
  uses a stable whole-quad UV bake box, adjusts tiny bake rectangles, samples
  texel centres, and perspective-corrects UV via `(u/z,v/z,1/z)` in its screen
  renderer. Neutral brightness is **64**, with real over-brightening above 64.
  Hardware polygon pen 0 is opaque; it must not become a hole when its RGB is
  black. System 22 objectflags select a single palette pen, with flags 1–2
  using low 15 cz_adjust bits and disabling shade. No centre-texel substitute.

## Fog and composition ordering

[`fog_hw.h/.c`](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/fog_hw.c#L117)
and [`quad_gl.c` fog paths](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/quad_gl.c#L844)
show Super 22 `texel → cmode pen → palette → brightness → CZ fog → polyfade`;
the System 22 renderer configuration supports fog **before** shade. Raw colour
bit 15 and cz_adjust bit 23 disable CZ fog. cz_type selects a bank through
czattr[6]; bank enable/reverse/delta and non-monotonic CZ table handling matter.
CZ depth is original native view depth, not the new eye depth. Stereo policy
for that depth and brightness interpolation still needs native acceptance.

[`ss22_gl.c` prepare/draw](https://github.com/spacestate1/namco22-decompile/blob/a9f1a0a1f9de62387a7138006fa36933fd477123/engine/ss22_gl.c#L110)
loads palette/fog/text/sprite state during preparation, collects polygons and
sorts them. Drawing clears mixer background, merges polygons/sprites by 24-bit
far-to-near order (sprite wins equal z), screen-fades, draws text including
priority-over-character interactions, then applies whole-frame gamma. Text's
screen-fade gate differs from polygons. GL depth for guns does not replace
native priority ordering. Fog, fades, gamma, text and sprites remain deferred.

## Ownership and initial bounded packet

Worker `VideoSnapshot` already owns palette/mixer/CZ/character/text/sprite banks;
`CapturedQuad` copies every native quad field before sorting. Native texture
tilemap, tiles and board addressing policy are not in that snapshot and are
not approved to load. An eventual owner-thread prepare must combine authorized
immutable session texture data with the **same tick's** copied palette/regs and
quad metadata, then publish a lease. Replay must neither re-bake from mutable
native globals nor reuse an atlas page another frame can overwrite.

The initial packet owns sparse decoded map cells, full 16x16 pen tiles,
palette entries and raw material selectors. Cell indices/tile IDs/attributes
are actual native concepts; this is a bounded synthetic representation, not a
new asset format or invented native atlas metadata. Caller must explicitly
declare tile16 versus extended addressing; missing referenced cells/tiles fail.
No filesystem/content input API. Preparation copies the packet into the frame.
CPU per-eye rendering carries UV and brightness through clipping and perspective
interpolation and evaluates texture/palette/shade only. Fog and remaining
composition are not implicitly enabled. The [GL material slice](GL-MATERIALS.md)
adds bounded exact rectangle upload/draw/lifecycle source with CPU dispatch
checks; real factory/provider/driver admission remains unaccepted.

Packet limits are 4096 cells, 4096 tiles, 4096 materials, a complete 32768-entry
palette and finite vertex UV magnitude <=65536 / brightness 0..255. These are
synthetic admission bounds, not a claim that every native scene fits. Cells/tiles
must be strictly sorted by actual native index. The decoder has no cache, no
sampling tiers and no atlas; each CPU fragment looks up its pen directly.
An extended tile beyond this pin's fixed 16 MiB limit resolves raw pen 0 like
the generic source. A missing in-range tile/cell returns BAD_ARGUMENT.

`MaterialVertex` supplies texel-centre coordinates (native integer UV plus 0.5)
before perspective division. The CPU interpolation uses the **new eye's** W,
including attributes created by homogeneous clipping; it does not apply the
original native inverse depth again. New-eye perspective brightness interpolation
and floating shade truncation are initial choices, not native raster parity.
CZ, fades, gamma, sprite/text priority and per-native-quad clip policy are absent
from this packet stage; raw capture retains those fields for the later adapter.
A CPU draw returning an error may leave partial pixels; callers discard that
output. Successful frame preparation replaces the previous frame atomically.

`patches/materials-source.json` and `tools/verify_material_source.py` guard the
seven generic evidence files by commit, normalized-text hash, exact allowlist,
clean selected-file status and path resolution. Optional pinned-source CMake
checks these alongside the existing 13 hook edits; no new upstream edits.
Synthetic evidence guard tests cover source drift and unauthorized path expansion.
The MIT upstream notice remains preserved in `patches/UPSTREAM-LICENSE`.
