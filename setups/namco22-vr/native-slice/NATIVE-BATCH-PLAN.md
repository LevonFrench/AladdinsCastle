# Native varying/batch slice — source contract and proof plan

PR53's accepted translation-only image and PR55's clipped CPU packets remain
separate frozen checkpoints. This branch connects packet emission to the pinned
upstream `quad_gl` batch, preserving its texture bake, palette, shade/poly-fade
and painter queue. No native unit or dependency is added. `begin_eye` and backend
graphics capabilities are unchanged. A new internal draw entry is compile-gated
off by default and is never invoked by the existing fixture draw route.

Supported source path: already clipped ordinary opaque textured Super22 world
fragments, at most9 vertices, under the native640x480 orthographic matrix and
existing owner-thread compatibility GL pass. It uses4-component homogeneous
positions in the native batch, one eye-W perspective correction for primary
baked/atlas UV and brightness, and a separate4-component original-board-depth
coordinate for CZ. That coordinate is `boardZ/2^21`, with Q1, for an8192-entry
nearest-filtered clamped1D CZ-alpha table on texture unit1. Table bytes are
derived with upstream `eng_fog_alpha`, including delta/constant policies.
The lookup occurs after perspective interpolation of board Z; eye W and painter
key never replace board depth, and already-looked-up vertex fog is not interpolated.

The range/filter contract was checked against the [OpenGL2.1 specification,
sections3.8.7–3.8.9](https://registry.khronos.org/OpenGL/specs/gl/glspec21.pdf).
For width8192, normalized depth `Z/2^21` gives nearest index `floor(Z/256)`;
edge clamp selects index0 or8191 outside the table. Both filters are nearest,
level0 has no border, and no mipmaps are used. Native delta range[-256,255] and
constant alpha[-1,255] are validated. Single-precision threshold behavior remains
part of the future GPU proof, not silently treated as byte-exact hardware parity.

The existing native shade-then-fog/poly-fade order is retained for Super22. The
native mixer/gamma/text/sprite passes are not integrated here. System22 fog-before-
shade, solid/prio/transparent quads, separate HUD, two-pass fog fallback, widened
backdrops and GL_LESS are rejected in this internal path. Colour stays in native
painter order; homogeneous clip Z is retained for a later shared-depth write
gate. This slice does not implement gun depth writes or accept gun occlusion.

Buffers and immutable source/fragment storage must not overlap. CPU packet
preflight precedes native emission. Native texture-bake/allocation or varying
failure must reject the fragment, not fall back to the old depth/UV formula.
Future eye publication must preflight all required resources and abandon a failed
target before presentation. Reused fog-table storage must flush earlier native
batches before replacement; stale texture/client-array state must be restored
before legacy draws/end. Core/GLES contexts and insufficient1D texture size or
missing client multitexture functions cannot be admitted.
The proof-only path assumes one GL context lifetime per process, the native pass
with unit1 client arrays/1D sampling initially off, and default pixel-transfer
scale/bias. It scopes unit1 texture matrix and upload unpack state; it does not
claim restoration of arbitrary external GL state or context-loss recovery.
The internal entry rejects contexts below3.3 compatibility, active GLSL/VBO/VAO,
fixed lighting/fog/culling/depth testing, a second projection/model transform,
and moved native HUD quads. It requires the native640x480 ortho and identity
modelview; packet eye projection must not be multiplied a second time.

CPU acceptance now: guarded source/strict compile, actual native fan expansion
and buffer copies through a CPU capture seam, identical legacy two-component
copy path, native bake/atlas coordinate conversion, key/order adjacency, finite
varying bounds and no partially published packet. The regression must use rotated
plus Z-translated near clipping and prove perspective-interpolated original board
Z differs from eye W and changes the actual native CZ result. GL entry rejects
when the compile gate/context is absent. No GL context is created in these tests.

GPU proof is pending separate lead review/coordination. Proposed bounded check:
one synthetic own-window pass with legacy and new packet paths separately
identified; eye rotation/Z translation across a near plane; nonlinear CZ band
and perspective checker; pixel/depth/array-state/order checks; GL errors fatal;
fresh private output and normal owned-window shutdown. Do not execute this plan
or enable `begin_eye` merely because source/CPU checks pass. No ROM/content loader,
game, OpenXR, headset, install or download is part of this proof.
