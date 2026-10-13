# Owned Super22 mixer background

Explicit `BackgroundPolicy::Super22Mixer` copies RGB from bytes0x08..0x0a of the
already owned `VideoSnapshot` mixer bank. The bank must have its existing exact
0x400-byte length and a nonzero tick; failed copy leaves prior output unchanged.
The private state is separate from fog/material/fade/gamma and contains only
policy, tick and three RGB bytes. `Absent` keeps black even with unused RGB bytes.
Unknown policy is UNSUPPORTED; present state must match the prepared Frame tick.

Frame preparation validates/copies that value into the usual immutable lease.
CPU/GL draw validates again before any upload/clear and uses frozen RGB for both
eyes and changed-pose replay. Producer mutation and subsequent frames cannot
change the active value. No source pointers, native globals or file reads.

The existing world's scissored clear uses that RGB, depth1 and opaque GL alpha1.
Geometry/backdrops replace covered pixels as before. Exactly one existing clear
per eye remains; no new pass, resource, dispatch function or shared ABI. The
CPU HUD-only image still clears black. Existing GL attribute restoration restores
the caller's ClearColor/masks/scissor/depth state on success and draw failure;
the target colour/shared depth survive for the runtime's next gun pass.

Authority is generic `namco22-decompile` pin
`a9f1a0a1f9de62387a7138006fa36933fd477123`: `engine/fog_hw.c` mixer background
fields and `engine/ss22_gl.c:492-493`, where background clears before world/sprite
composition, screen fade, text and final gamma. Existing source/hash guards are
unchanged. This does not classify real layers or implement those later stages.

Synthetic CPU tests check exact RGB bytes, malformed bank lengths/zero ticks,
atomic copy/preparation, present/absent/unknown policies, same-tick ownership,
nonzero paired eye rectangles and untouched neighbours, covered/uncovered
pixels, identical depth/raycast, HUD-only black and subsequent backend leases.
GL CPU mocks verify exact normalized ClearColor, one clear, opaque alpha,
unchanged fogged material output and hostile caller state restoration, with no
new generated/deleted objects. Typed-header tests copy from a real-shaped leased
synthetic source bank. No actual driver, context/window, GPU or game executes.

Graphics query mask/native board admission remain zero. Fades/gamma are not
folded into this clear. Native ordering, layer classification, full composition,
world scale, driver/gameplay/headset and Board1 acceptance remain open. SDK,
game/generated-source/BIOS access, downloads, installs and GPU work still need
owner approval.
