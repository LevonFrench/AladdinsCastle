# Hub launch lifecycle candidate

Authored `openxr` launches now use the existing current-runtime policy. Only a
SteamVR-pinned route changes the child's runtime environment. Cooperative close
remembers cancellation before spawn, retains the payload lease while queued or
running preparation unwinds, and waits for the Windows child's own window before
requesting normal close. It adds no force deadline. The real Hub's global Stop
uses that policy for Pinball, cancels pending restart, coalesces repeated close,
and explains when native close is unavailable.

Failing-first synthetic tests cover alias admission and pre-child cancellation;
real-QML clicks exposed the previous forced Pinball Stop path. The native hidden
window regression also fails both cases with previous close behavior restored
as a negative control: each child times out instead of receiving normal close.
No game contents, visible desktop window or graphics runtime is used.

The actual launch CTest harness passed all 92 cases without skips. The complete
offscreen/software UI suite passed 49 cases, with no failures and one intentional
headed benchmark skip. An earlier full UI run failed on eight GameArt Repeater
TypeErrors. A controlled pending-record reproduction produced the same eight
errors; a null-safe field access fixes it and the test verifies all eight colors
after the record arrives. Warning capture remains unchanged. The precise
scheduling cause of the earlier intermittent failure remains unproved.

This is source/CPU evidence, pending independent exact-head review and hosted
Windows/Linux checks. Actual upright compositor content, native Pinball VR player
startup, both-eye capture and headset acceptance remain separate open gates.
The next booked runtime work follows the preceding consumers' explicit closure.
