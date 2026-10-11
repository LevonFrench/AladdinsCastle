# Head-pose clipping stage — synthetic CPU contract

This follow-on stages native prepared polygons for a later per-eye raster draw.
PR53's translation-only executable/image and branch are frozen. This branch
does not admit a new GPU draw, game, backend callback table or OpenXR session.
The existing `acn22_begin_eye` remains restricted until native attribute emission
is verified separately. The new stage is not a replacement texture/fog renderer.

Supported input is immutable captured native raw view XYZ from an ordinary
four-vertex polygon whose original board depths are positive. Native painter
key/order, palette/texture selectors and CZ flags stay attached. Captured +Z
forward is converted to canonical -Z once. The supplied `acvr_eye` applies a
finite affine orientation, XYZ translation and positive uniform scene scale,
followed by a standard GL off-axis perspective matrix (finite or infinite far).
Eye viewport is a positive bounded rectangle, layer0. No native projection is
multiplied into it; no per-eye sort is performed.
Coordinates are finite within the positive-depth signed32-bit capture range;
UV is in[0,4096), brightness in[0,32768], and uniform scale is in[1e-9,1e9].
Matrix columns must agree in squared norm and orthogonality within3e-5 relative
tolerance. View affine row and standard projection structure are checked exactly.
Rectangles are bounded at16384 per origin/dimension. Failure returns a negative
result with written count0 and unchanged output bytes; capacity failure is
`ACVR_BAD_ARGUMENT`, unsupported source/matrix is `ACVR_UNSUPPORTED`.

The stage bounds input at1024 quads and output at two fragments per source quad.
The upstream fan's two triangles are clipped independently against all six
homogeneous GL planes, before dividing by W. This preserves the original
diagonal and attribute interpolation for nonplanar quads. Each clipped fragment
has at most nine vertices; complete culling succeeds with no fragment. An
eye-plane projected fan area at or below1e-14 NDC units is treated as numerical
zero and culled; a truly degenerate source triangle is unsupported. An
unsupported/invalid polygon aborts staging of the entire queue, rather than
silently disappearing or exposing half a stereo pair. Source memory and native
frame counter are unchanged. All fragments for a polygon stay adjacent in its
prepared painter position, including stable reverse-emission ties.

Every clip intersection uses one edge parameter for clip XYZW, UV, brightness
and **original board depth**. Eye depth never replaces board depth. The stage
retains native UF/VF/BF precision from the ordinary, unclipped native polygon.
At a raster sample, attributes are reconstructed as
`sum(lambda*attribute/W) / sum(lambda/W)`. UV and board depth share that same
denominator. A CZ lookup uses reconstructed board depth and unchanged native
CZ state, not the eye's near/far value or native painter key. Nonlinear CZ lookup
is not equivalent to interpolating lookup results; tests must distinguish them.

The downstream fixed-function handoff is homogeneous position
`(screenX*W, screenY*W, -clipZ, W)` under the native640x480 orthographic matrix,
and texture coordinate `(U, V, 0, 1)` before native bake/atlas conversion.
GPU perspective interpolation then divides by eye W once. Supplying native
`U/boardZ, V/boardZ, 1/boardZ` as well would introduce a second division and is
not admitted. Board depth/brightness are independent varyings. Connecting those
varyings through native shade/fog/batching and preserving painter depth writes
is a later source/GL acceptance gate, not established by this CPU packet.

Unsupported: direct/sprite/text/spot/HUD polygons, originally behind/near-clipped
or guard-clipped native polygons, partial native scene windows, absent capture
metadata, nonfinite/degenerate input, reflection, nonuniform scale/shear,
nonstandard projection/depth conventions, other layers and unbounded queues.
The stage cannot recover geometry culled by the original game/board and does
not establish head-turn coverage. Palette, material sampling, CZ evaluation,
gamma, shared gun depth, raycast and game calibration remain native integration
work. The provisional1000units/metre policy is unchanged.

CPU adversarial fixtures must cover eye near/side/far crossings, W<=0 vertices,
fully hidden triangles, exact boundary points, rigid rotation plus Z translation,
uniform scene scale, asymmetric frusta, independent board/eye depth,
perspective-correct UV/depth at interior samples, nonlinear CZ witness,
unchanged tie/priority metadata, replay immutability, malformed structs/matrices,
capacity failure and no partially published result. The new queue stage allocates
no heap memory: it validates/counts first and publishes on a deterministic second
pass.
The internal stage API requires source, output and written-count storage to be
distinct and nonoverlapping, in addition to immutable owner-thread source/eye
input throughout both passes. The controlled fixture uses distinct buffers;
this API is not admitted for arbitrary untrusted caller storage.
Native capture allocation failure marks the fixture frame inadmissible before
either legacy draw or new CPU staging; it never admits a partial queue.
That source guard is not proof of arbitrary native allocation failure recovery.

Current local verification: both CTests pass, with5878 head-pose assertions,
actual native prepared-queue staging, strict own-C compilation and guarded
patch-boundary tests. No content loader was invoked; no game data, GPU or new image was used.
The twelve upstream translation units and cached dependency boundary are unchanged.
