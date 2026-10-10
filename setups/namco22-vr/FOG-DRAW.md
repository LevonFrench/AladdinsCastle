# Bounded Super22 fog drawing

This slice draws enabled fog only on explicitly textured, opaque Super22 World
triangles (`objectflags=0`). Private CPU and compatibility GL paths consume the
same owned lease. Enabled System22 constant fog, solids, flat colour, HUD,
backdrop and retained GunFlash triangles return UNSUPPORTED before target
clear/upload; explicitly unfogged metadata preserves the prior draw path.
Scene preparation still supports those other policies for state inspection.
The synthetic preparation policy discards GunFlash polygons before drawing.
Graphics query mask and native game admission remain zero; shared ABI is unchanged.

## Native weights and current eyes

`prepare` evaluates three decisions once per source triangle from the owned
[fog packet](FOG-STATE.md), storing byte unfogged weights and RGB in private
`Triangle::fog_samples`. Original integer native depth stays separate from scene
coordinates. Both eyes/replays reuse those values without rereading mutable
source state, decoding CZ or substituting eye-space Z. Immutable Frame values
must not be edited during a lease, as for materials and geometry.

CPU homogeneous clipping interpolates the weights at generated eye clip
vertices, then interpolates per fragment with the current eye's inverse W.
GL submits the unchanged weights through primary colour alpha with smooth
shading. This is a deliberate stereo extension of the generic renderer's native
vertex approximation, not per-fragment CZ lookup or native pixel parity.

## Colour, saturation and shared depth

The GL path follows pinned generic `engine/quad_gl.c:88-197` Super22 mode 2:
unit 0 modulates exact neutral palette texels by brightness/256, RGB_SCALE=4;
unit 1 interpolates that result toward unshaded fog RGB, RGB_SCALE=1. Primary
alpha selects the unfogged weight. Both stages replace output alpha with opaque
texture/previous alpha, so fog never becomes transparency or priority alpha.

The floating CPU reference is, for each byte channel:

```
shaded = clamp(neutral_texel * interpolated_brightness / 64, 0, 255)
out = floor(clamp(shaded * unfogged + fog_rgb * (1 - unfogged), 0, 255))
```

It samples at brightness 64 to retrieve neutral palette bytes, retaining
floating shade until the final floor. The regression `(texel=1, brightness=127.5,
fog=10, unfogged=127/255)` yields 6; pre-quantizing shade would yield 5.
High-brightness checks distinguish shade-clamp-before-fog from a final-only clamp.
GL explicitly forces and restores CLAMP_FRAGMENT_COLOR=TRUE, since hostile
caller state can otherwise disable intermediate combiner saturation.
The separate [polygon-fade source policy](POLYGON-FADE.md) scales shade and raw
fog inputs before this saturation, preserving floats until final output.
CPU analytic comparisons allow one output byte for floating raster interpolation;
driver quantization and coverage have not been measured.

No overlay pass, blend transparency, geometry changes or extra clear is added.
Each eye keeps its existing scoped clear, LEQUAL test and shared depth writes.
Fog changes RGB only; all written CPU depths and ray/gun intersections match the
unfogged fixture. Native priority-alpha, polygon/sprite merge order and ties are
not recreated by these depth-separated synthetic checks.

## Resources, state and evidence

One private complete white 1x1 RGBA texture drives unit 1. Its four bytes and
one object count are reserved inside the existing 32 MiB/256-object lease
budgets before pixel allocation. It uploads transactionally with material
textures before clear, replays unchanged and retires on the owner thread with
the lease. No borrowed attachment is retained/deleted or context created.

Unit 1 binding, sampler, matrix, all combiner parameters and constant colour,
original active selector and fragment-clamp state are saved/restored, including
partial setup, upload and draw failures. Existing unpack/transfer, modern enable,
target identity, matrices and owner-thread guards remain. Required new borrowed
dispatch entries are TexEnvfv, GetTexEnviv, GetTexEnvfv and ClampColor.

`namco22_analytic_fog_draw` uses independent ray/triangle intersections to check
asymmetric perspective weights across a near-clipped triangle and moved eyes,
plus endpoints, gates/admission, saturation/rounding, depth/raycast, occlusion and
dummy budget rejection. GL CPU mocks check combiner inputs/output opacity,
same-draw submission, mixed fog transitions, exact uploads, replay, hostile
unit-1/clamp state, errors and exactly-once owner cleanup through real callbacks.
No driver/library/context, GPU, native engine or game fixture executes.

API basis: [Khronos GL3.3 compatibility specification](https://registry.khronos.org/OpenGL/specs/gl/glspec33.compatibility.pdf),
sections 3.6.1 (current-eye attribute/depth interpolation), 3.9.16 (combiner
operations/clamping) and 3.7.5 (fragment colour clamp control).

Screen fade, final gamma, sprites/text/priority, actual layer
classification, world scale and native driver/gameplay/headset acceptance remain
open. Bounded polygon input fade is now explicit; screen fade/gamma remain distinct
future composition operations, not palette or fog substitutions. This cannot admit full Time
Crisis composition. Owner approval is still required for GPU use, game/BIOS/
generated-source access, downloads and installs.
