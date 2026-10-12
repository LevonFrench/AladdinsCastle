# R4 native adapter compile and source publication — 2026-10-11

Reviewed source commits 5ad7947/5c6b067 are imported into a current-next branch;
the old branch is preserved. The adapter and paired proof stay default-OFF, and
the production renderer's graphics capability remains mask0. No shared API/core
changes, game material, validators or packaging are included.

The isolated adapter library and factory link target compiled on the first attempt
with two workers, existing GCC/SDL2 and the current reviewed header: 22/22 build
steps, exit0. No GPU/context/window was initialized. After call-path review, the
small factory executable ran exactly once and returned0: table/function exports,
caller size/version/tail canaries, partial-tail rejection, and null-backend early
errors. It never calls open/step or reaches a GL/context probe. No broader CTest or
second build was run. The compiled factory/adapter source and API helper/header
match the new branch byte-for-byte. Slot released; scoped process audit found no
adapter compiler processes remaining. Private logs/outputs remain private.

Source repairs preserve all borrowed-target draw selectors and reject nonzero
colour attachment levels. Actual GL target/state/resource cleanup is not proved
by the compile or factory test. Staging/target-state regressions, borrowed-target
pixels, real-game composition and headset behavior remain separate gates.

The current static adapter only feeds synthetic formula fixtures, and the
existing proof executable does the same. Real Time Crisis still needs a live
board polygon/camera feed plus a caller using the borrowed-context two-eye
prepare/draw path. This is an entry-point gap, not a request for more validators.
Owner holds GPU/runtime; Guns owns the heavy CPU slot. Darkside source preflight
is retained until runtime becomes available.
