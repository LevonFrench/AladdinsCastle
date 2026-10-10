# Per-eye desktop GL source path

The scene renderer uses the existing immutable `Frame` and `game_draw_eye`
signature. `configure_gl_draw` binds a borrowed dispatch table through the
runtime device's resolver; it creates no context, window or library handle.
Query **still advertises graphics mask 0** and ordinary synthetic open has no
graphics binding. A private integration binding exercises the compiled draw
callback in CPU mock tests. Real provider/engine admission remains unaccepted.

## Provider and attachment requirements

- Desktop **OpenGL >=3.3 compatibility profile** for the combined world/gun
  path. World drawing uses compatibility functions; the lead's separate gun
  renderer uses GL3.3. Device must declare `ACVR_DEVICE_GL_COMPATIBILITY`,
  never GLES, and supply a nonzero context handle at initialization. The draw
  preflight also verifies version/profile/stack support.
- Single-sample, single-layer complete supplied FBO, colour at attachment 0,
  runtime-owned shared depth image and format, with at least 16 depth bits.
  The eventual real backend must declare `ACVR_CAP_REQUIRES_SHARED_DEPTH`.
  Missing depth is rejected before any draw/clear. Default-window FBO and
  private depth fallback are not supported by this combined path.
  Colour/depth image names must be nonzero and fit GL object width. Attachment
  0 and depth must be texture objects whose names match the supplied target;
  completeness or sufficient depth precision alone does not admit a target.
- The provider owns depth/colour acquisition, synchronization and presentation.
  Backend never detaches, deletes or clears shared depth after scene drawing.
  Guns reuse it through the same FBO/projection. Desktop preview needs a host
  to blit the supplied FBO to its window; this module does not swap or present.
- Column-major canonical RH GL forward-depth perspective. Reverse-Z, GLES,
  core-only, Vulkan, D3D, multiview, MSAA and separate HUD are rejected.

The source draws opaque packed display-referred RGB triangles or the
[bounded lease texture/material path](GL-MATERIALS.md), including the CPU path's
explicitly flattened HUD. Infinite backdrop directions ignore
translation and use a far-plane projection before the world pass. The supplied
eye view/projection are used directly, without another native camera projection
or scale. No emulation, input, raycast or output queue changes occur while drawing.

This is not native composition parity: real texture admission/fog/palette-gamma/sprite
priorities, real HUD classification, culling expansion and per-game calibration
are still required. Frame colour bytes are written without framebuffer sRGB
re-encoding; the gun renderer must establish its own linear/encoded colour policy.
The initial provider uses ordinary GLSL program binding, with programmable
pipelines and legacy assembly-program modes inactive.

## State and errors

Only the eye's lower-left rectangle is cleared, with scissor enabled and both
colour/depth write masks set. Forward depth uses clear 1, LEQUAL and range 0..1.
All fixed-function texture units and inherited raster/clip effects are disabled.
This includes rectangle textures on every unit, colour sum, rasterizer discard,
depth clamp, sample mask and all context clip distances. Modern enables, clip
distances and per-unit rectangle enables are explicitly saved/restored instead
of assuming the legacy attribute stack covers them.
Context attributes, model/projection matrices and stack depth, draw/read FBOs,
GLSL program, texture selector and target draw-buffer selection are restored.
This restoration does not erase target colour/depth contents. Borrowed target
handles are never retained or deleted. Backend-owned material textures are
uploaded once per immutable lease and retired during release/close on the owner
context; there are no swaps. Unit0 texture matrix/binding/sampler, pixel-unpack
buffer/state, compatibility pixel transfer and current UV/colour state restore
alongside legacy attributes. Optional imaging-subset transfer stages must be
inactive with identity colour matrix and canonical post-transfer scale/bias.

Caller enters with a clean error queue. A pre-existing error is captured as an
entry diagnostic and returns BAD_STATE without binding or drawing; the queue is
not drained. Draw/setup errors are returned and preserved, with a separate
restore-error field. Missing dispatch entries, unsafe headers/rectangles, wrong
frame IDs or wrong thread fail before drawing. EXT FBO and ARB active-texture
resolution fallbacks are explicit; Windows sentinel addresses are rejected.

Mock tests use local CPU functions for every GL address. They cover resolver
fallback, profile/shared-depth admission, actual callback/lease use, paired and
replayed eyes, newest matrices, scoped clears, raw triangle submission, infinity,
multitexture/depth/colour/raster/matrix/FBO/program state and partial failure/error
restoration. They prove source/control-flow behavior only. They do not compile a
driver shader, validate rasterized pixels, prove gun occlusion or run graphics.
Texture tests retain upload bytes and compare against CPU packet sampling;
generation/upload rollback, lease replay and release/close lifecycle are mocked.
The hostile-state fixture deliberately omits modern/clip/rectangle restoration
from mocked legacy `PopAttrib`; identity/type/query-error attachment cases must
reject before clear and restore the original caller state.
