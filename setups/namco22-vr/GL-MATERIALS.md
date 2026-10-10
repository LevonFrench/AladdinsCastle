# GL material resources — bounded lease design

Branch starts from `next` after accepted PR #17 merged at `469ca13`.
Exact packet head `94a67f0` passed independent six-suite pinned-source review
and all final-head hosted Windows/Linux/catalog push and PR checks. This task
adds compiled GL upload/draw/lifecycle behavior; driver/gameplay proof stays open.

## Resource and texture strategy

Each immutable frame lease owns the logical texture set. The renderer prepares
exact RGB rectangles from triangle UV extents and material selectors; all texels
are sampled from that same owned packet at neutral brightness. Identical extent/
material keys share one private texture within the lease. Solids use one exact
palette pixel. This is private renderer storage, not inferred native atlas data.
Never substitute a centre sample or decimate an unsupported span.

Initial admission limits: edge 1024, 256 distinct rectangles, 65536 triangles,
total RGBA storage 32 MiB per lease. All checked rectangle/count/byte arithmetic
finishes before pixel allocation; GL_MAX_TEXTURE_SIZE is checked before upload.
Missing sparse data or larger spans fail before target clear.
These are provisional bounded source limits, not proof every native scene fits.
The CPU pixels are staged first; GL creates/uploads all textures transactionally
on the first draw, then both eyes and changed-pose replay reuse immutable handles.
No target colour/depth/FBO is retained. New-frame drawing cannot replace an
unreleased texture lease. A failed upload rolls back only owned object names.

Textured triangles use the supplied eye matrices, normalized texel-centre UVs
and nearest sampling. GL supplies new-eye perspective interpolation; do not
divide by original native Z again. Compatibility texture combine modulates at
RGB scale 4 with brightness/256, preserving brightness above neutral 64. Opaque
pen0 stays alpha255. Solid no-shade flags use neutral brightness. Flat triangles
keep the explicit flat path. Fog/fades/gamma/sprite/text priority stay unsupported.

## Context, cleanup and state

The existing contract requires serial owner-thread callbacks with the borrowed
context current and alive through frame release/backend close. No shared ABI
change is needed. Initialization only resolves dispatch addresses. Upload/draw/
release/shutdown use that owner thread; wrong-thread private calls reject before
GL commands. Explicit private shutdown diagnoses lifecycle misuse; no destructor
creates a context, dispatches GL or silently moves deletion to another thread.
Valid public frame release/close attempt cleanup and complete their void
contract. Ordered GL texture deletion marks owned names for driver retirement;
no CPU lease release is treated as a GPU-completion fence. No wait/flush/swap.

Existing profile/attachment identity/shared-depth and inherited raster checks
remain. In addition, save/restore unit0 texture binding, environment, matrix and
sampler, pixel-unpack buffer plus alignment/row/image/skip/byte-order fields;
normalize them for uploads. No borrowed attachment object can be deleted.
Compatibility base pixel-transfer scales/bias and colour maps are neutralized
under the saved attribute state. Optional ARB_imaging stages must be disabled,
with identity colour matrix and canonical post-convolution/matrix transfer;
noncanonical optional state returns UNSUPPORTED before any upload/clear.
State restoration must work after partial setup/upload/draw failure. Preserve
first draw/upload error and separate cleanup/restore diagnostics.

CPU mocks retain every uploaded byte, model hostile unpack/sampler/matrix
state and verify stable replay, exact texels/UV/brightness, owned-only rollback/
release/close, wrong thread, allocation/upload failures and target preservation.
They do not execute driver texture allocation/rasterization or prove pixels.

Release/shutdown return private results for owner/lifecycle diagnostics. The
public void callbacks finish cleanup on valid owner-thread calls even if deletion
reports an error; the renderer keeps first failure and separate cleanup/restore
fields. Wrong-thread public release/close return without dropping the handle,
so the owner can retry; these are invalid calls under the serial callback contract.
No GL commands run in the renderer destructor. Standalone users must call explicit
shutdown while the borrowed context is current; renderer copying/moving is disabled.

API basis: [Khronos GL texture environment specification](https://registry.khronos.org/OpenGL/specs/gl/glspec13.pdf)
and [GL3.3 compatibility specification](https://registry.khronos.org/OpenGL/specs/gl/glspec33.compatibility.pdf).
