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
  never GLES. The draw preflight also verifies version/profile/stack support.
- Single-sample, single-layer complete supplied FBO, colour at attachment 0,
  runtime-owned shared depth image and format, with at least 16 depth bits.
  The eventual real backend must declare `ACVR_CAP_REQUIRES_SHARED_DEPTH`.
  Missing depth is rejected before any draw/clear. Default-window FBO and
  private depth fallback are not supported by this combined path.
- The provider owns depth/colour acquisition, synchronization and presentation.
  Backend never detaches, deletes or clears shared depth after scene drawing.
  Guns reuse it through the same FBO/projection. Desktop preview needs a host
  to blit the supplied FBO to its window; this module does not swap or present.
- Column-major canonical RH GL forward-depth perspective. Reverse-Z, GLES,
  core-only, Vulkan, D3D, multiview, MSAA and separate HUD are rejected.

The current source draws opaque packed display-referred RGB triangles, including
the CPU path's explicitly flattened HUD. Infinite backdrop directions ignore
translation and use a far-plane projection before the world pass. The supplied
eye view/projection are used directly, without another native camera projection
or scale. No emulation, input, raycast or output queue changes occur while drawing.

This is not native composition parity: texture/material/fog/palette-gamma/sprite
priorities, real HUD classification, culling expansion and per-game calibration
are still required. Frame colour bytes are written without framebuffer sRGB
re-encoding; the gun renderer must establish its own linear/encoded colour policy.
The initial provider uses ordinary GLSL program binding, with programmable
pipelines and legacy assembly-program modes inactive.

## State and errors

Only the eye's lower-left rectangle is cleared, with scissor enabled and both
colour/depth write masks set. Forward depth uses clear 1, LEQUAL and range 0..1.
All fixed-function texture units and inherited raster/clip effects are disabled.
Context attributes, model/projection matrices and stack depth, draw/read FBOs,
GLSL program, texture selector and target draw-buffer selection are restored.
This restoration does not erase target colour/depth contents. No GL objects or
borrowed target handles are retained; no resource allocation/deletion or swaps.

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
