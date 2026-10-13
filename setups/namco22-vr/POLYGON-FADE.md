# Pinned Super22 polygon-fade input order

`PolygonFadePolicy::Super22InputFold` is an explicit private choice to follow the
pinned generic renderer's input order. It is **not hardware-parity proof**.
Present state owns tick and mixer RGB bytes0..2; the copy helper reads only the
already owned 0x400-byte mixer bank after length/nonzero-tick validation. Failed
copy/preparation preserves prior output. Both draw paths require the same Frame
tick. Absent policy is identity. Unknown policy rejects before upload/clear.

Authority: generic `namco22-decompile` at
`a9f1a0a1f9de62387a7138006fa36933fd477123`, `engine/fog_hw.c:168-169` and
`engine/ss22_gl.c:61-67,498-501`. Polygon fade enables globally only when valid
mixer data has at least one of bytes0..2 different from255. All255 is a disabled
sentinel: identity, not three255/256 factors. When enabled, **every** channel uses
byte/256, including255. Screen-fade flags/factor and CZ fog gates do not gate it.

## Float inputs and saturation

The pin folds each fade factor into primary shade at `quad_gl.c:824-827` before
unit0 texture modulation/RGB_SCALE4 saturation, and separately into raw fog RGB
at `quad_gl.c:748` before unit1 interpolation. This differs from applying fade
after an already-clamped shade/fog result, even though the source comment calls
it fade-after-fog. The approved order is explicit rather than inferred hardware.

For each byte channel T, current-eye brightness B, original unfogged weight A,
raw fog byte F and D=1 absent/disabled, otherwise fade byte/256:

```
S = clamp(T * B/64 * D, 0, 255)
out = floor(clamp(S * A + F * D * (1 - A), 0, 255))
```

CPU samples neutral palette bytes at brightness64, retains floating shade and
scaled fog through blending, and floors only the final byte. GL submits primary
RGB brightness/256 times each D and raw fog constant F/255 times D. Raw material,
fog and fade bytes remain unmodified. The constant frame factors commute with
current-eye homogeneous clipping/perspective interpolation; native depth and
cached unfogged weights stay unchanged. Driver rounding remains unmeasured.

Required fragment clamping is explicitly saved/forced/restored for admitted
active fade as well as fog, including absent/disabled fog and partial setup/draw
failure. No extra texture/unit/pass/dispatch or resource lifetime is introduced.
Existing RGB-only behaviour preserves opaque alpha, shared depth/raycast, the
owned background clear and one clear per eye. Zero fade paints covered geometry
black while retaining its depth; uncovered background is untouched.

## Bounded admission and checks

Active fade admits only explicit opaque textured World triangles, objectflags0,
with absent fog or the accepted Super22 table policy. It rejects retained
HUD/backdrop/GunFlash, flat/solid, System22 constant policy, unknown order or bad
tick before uploads/clear; unsupported fade is never silently omitted. Existing
synthetic preparation still discards GunFlash source polygons. Inactive/absent
fade retains earlier admission/pixels; empty worlds leave background unchanged.
State-only preparation may represent combinations that draw admission rejects.

Hand-derived red-channel witnesses use T200/B128/F40/A127/255:
fade128 gives109 (not after-clamp73 or shade-only119), fade192 gives142 (not
after-clamp110 or final-only164). Unsaturated T64/B64/A1/all255 gives64, not63;
enabled RGB(255,128,255) gives red63. T1/B127.5/F10/A127/255/fade128 gives3 from
39245/13056, not premature-byte2. Tests use independent literal/integer rational
expectations, plus full/no fog, zeros, mixed channels and both fog-disable gates.

The independent ray/triangle oracle also checks a near-clipped asymmetric
triangle under three eye poses with known per-channel factors, allowing one byte
RGB and 2e-6 depth for CPU raster arithmetic. GL CPU mocks cover hostile clamp,
no-fog gates, exact neutral uploads, immutable both-eye replay/new leases,
callback/owner cleanup and invalid/unsupported early rejection. Pinned-header
fixtures decode actual-shaped owned synthetic mixer copies, without native globals.
No driver, GL context/window, GPU, native engine or game is executed.

Screen fade, final gamma, sprite/text gates and native priority/order remain
distinct deferred composition stages. This polygon-only policy cannot admit a
full composed-screen request or native Time Crisis; graphics query/native board
admission remain zero. No shared ABI/factory/other-board expansion. World scale,
complete composition and driver/gameplay/headset acceptance remain open.
