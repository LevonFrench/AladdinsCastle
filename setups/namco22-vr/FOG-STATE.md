# Owned fog state and decision helpers

This block implements CPU state inspection only. Time Crisis is catalogued as
Super System22, so the Super22 CZ tables are relevant; no native board/factory
admission or Time Crisis fog pixels are proved. Rendering, sorting, native code
and shared libacvr ABI stay outside the original state-only block. Its successor
[bounded Super22 fog draw](FOG-DRAW.md) now prepares immutable vertex weights
from this packet; native game/driver acceptance remains open.

## Explicit policies and original depth

`FogPolicy::Absent` is distinguishable from an unfogged result. Explicit
`Super22Table` state decodes a supplied same-tick `VideoSnapshot`: four owned
8192-byte CZ tables, eight host-order attributes and mixer fog RGB bytes5..7.
Only the existing copied mixer/CZ arrays are read. No file loader, endian guess,
game detection, engine global, C oracle or original source execution.

`System22Constant` is a minimal caller-supplied seam. It requires an explicitly
provided alpha/colour, returns UNSUPPORTED when absent, and never derives the
constant from CZ tables, scene depth or a game name. An explicitly supplied -1
selects the generic unfogged fallback; 0..255 follows `eng_fog` semantics.
It does not construct a board constant or add a System22 runtime.

Each private `FogQuad` stores the original integer native depths separately from
reconstructed `Vec3`, HUD depth and eye matrices. The source-header helper copies
explicit fan indices from an already owned `CapturedQuad.rv`; it does not choose
guard-band vertices, sort, clip or convert coordinates. All raw selectors and tick
are copied. `SceneInput` preparation validates policy/tick/depth/constant binding
and copies state and quad data into `Frame`. No source pointers survive publication.
Private `inspect_fog_frame` reads that usual backend lease. Frame preparation also
caches decisions for the bounded draw, keeping source depth separate from eye Z.

## Source-exact decisions

Authority remains generic `namco22-decompile` at
`a9f1a0a1f9de62387a7138006fa36933fd477123`; existing material hash guards and
hook/header pins are unchanged. See source links in [MATERIALS.md](MATERIALS.md)
and exact evidence/ordering inventory in
[`docs/status/stereo/2026-10-10-6.md`](../../docs/status/stereo/2026-10-10-6.md).

CZ intervals preserve non-increasing-word skip behavior, including unchanged
previous value/extrema. Reverse visits `i XOR 255`; words clamp to8192, interval
and extrema fills are bounded within each owned table. Raw colour bit15 and
cz_adjust bit23 disable table fog. cz_type routes through czattr[6]; selected-bank
enable and word-sign delta semantics are retained. Alpha255 means unfogged;
alpha0 is fully fogged. Native depth clamps its `z>>8` index to0..8191.

The decision carries selected bank/delta/colour and resulting alpha; it has no
table pointer. Invalid bindings/indices return an error without replacing prior
decisions or prepared frames. Native depth is required for table policy even if
a disable gate is present, preserving explicit source-data admission.

Fixed synthetic expectations cover complete monotonic/reverse tables, equal/
decreasing entries, zero/overflow words, all bank routes/disable gates, unusual
delta words, native index/factor endpoints, explicit absent/constant policies,
same-tick binding and copied capture/frame/backend-lease ownership. A CPU draw
comparison confirms explicitly disabled fog changes neither pixels nor shared depth.

The successor draw verifies a bounded shade/fog clamp/rounding policy and eye
interpolation with synthetic CPU/GL mocks. Full brightness/fog/fade order, screen fade, gamma,
sprites/text/painter priority, native state initialization, world scale, real
driver/gameplay/headset acceptance remain unimplemented. The 32 KiB tables are
state storage, not a new GPU buffer or a claim of final native scene capacity.
