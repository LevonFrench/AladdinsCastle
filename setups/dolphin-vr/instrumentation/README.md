# Private draw instrumentation

This opt-in logger targets DolphinXR source commit
`85c4266b29eb308d173fefa0f21f5cb77df3516a`. It records the primary draw after
`DrawCurrentBatch` in `VertexManagerBase::RenderDrawCall`, excluding the optional
coverage prepass and draws skipped before submission. It changes no rendering,
projection, input or game settings. The patch tool refuses other source revisions
and existing edits in its two target files. Only an owned clone under `.local`
can be patched; the repository contains original instrumentation, not Dolphin code.

Run `python -B setups/dolphin-vr/instrumentation/apply.py <private-source>` before
building. Keep all build outputs, source clones, logs and game captures under
`.local`. Preserve the upstream notices and per-file SPDX headers. An existing
different logger requires inspection before replacing it. Do not patch a source
checkout while a build or application uses it.

Set `AC_DOLPHIN_DRAW_LOG` to a private CSV filename before launching the instrumented
build. Its parent directory must exist. An unset variable disables the logger.
Only every 30th emulated frame is sampled, with every primary draw in that frame,
up to 200,000 rows. The per-instance log closes on shutdown or when the limit is
reached; selected frames are flushed. Each launch needs a new filename because
the logger opens the file for replacement. Open/write/flush failures appear on
stderr. Check for a nonempty CSV before drawing conclusions.

Columns contain frame and submission ordinals, elapsed time, primitive/index
count, projection type, six raw projection coefficients, viewport, packed scissor,
depth, blend and generator state, plus the base vertex/pixel shader UID hashes.
Those hashes identify generated pipeline state, including uber variants; they
do not identify texture content or custom shader replacements. No vertices,
textures, pixels or emulated
memory are recorded. Projection type `0` is perspective; `1` is orthographic.
For perspective, FoV uses the raw game's off-centre frustum:
`atan((1 + offset) / scale) - atan((-1 + offset) / scale)` for each axis.
Invalid scales and orthographic FoV are `nan`. These are game projection angles,
not the effective headset view or evidence of game-side culling.

Run `python -B setups/dolphin-vr/instrumentation/summarize.py <private-csv>`;
optional frame bounds separate observed title and mission segments. The summary
groups raw states and reports depth/blend facts. Every render role remains
`unknown`: orthographic draws can be menus, HUD, backgrounds or effects, and
perspective draws can also contain overlays. Visual observation and correlation
are required before setting a game-specific HUD panel or wide-frame override.
Sampled metadata and wall time do not measure FPS; use a separate quiet run with
the trace disabled for performance results.

`test_trace.cpp` uses only synthetic raster values. Compile it with a C++20 host
compiler and run it with a private output directory. It checks off-centre FoV,
invalid projections, orthographic values, sampling, row cap, CSV columns and a
disabled logger. It does not exercise an emulator, game, graphics device or XR.
