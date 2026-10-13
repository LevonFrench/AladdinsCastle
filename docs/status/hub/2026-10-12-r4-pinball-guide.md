# Pinball user guide

Added a standalone, game-neutral Pinball guide covering folder selection,
VR-ready metadata limits, VPX selection, preparation cancellation, retained
now-playing, normal close, successful-close restart and recent-play history.
The existing software boundary links it and describes the same lifecycle.

Checked visible control names and behavior against the real QML page/card and
the independently reviewed PR97 controller/launcher source. No new tests are
needed for this documentation-only change; public privacy and whitespace checks
apply. Cancellation/global Stop wording depends on PR97 integration first.
No native VR, compositor, score persistence or headset claim is added.

PR97's source/CPU results remain separate from actual upright/Pinball acceptance.
The booked runtime item still follows the preceding consumer's explicit release.
