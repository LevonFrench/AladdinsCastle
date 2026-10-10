# Overlay diagnostics

The Hub's `--spike` scene is a diagnostic surface on the 1280 × 800 overlay
canvas. Four off-centre targets show individual hit counts. A marker shows the
most recent position submitted to Qt and its raw cursor ID. The long synthetic
card list shows its current `contentY`; it contains no game content.

Numeric rows separate each raw cursor ID's moves, presses, releases, smooth and
discrete scroll packets. They retain raw event type, last raw mouse coordinates,
Qt event type/position/button state, and each scroll kind's latest raw delta and
Qt angle delta. These are accepted packets submitted by the existing translator,
not proof that a controller hit its intended control. Invalid/unsupported packets
and translator-generated focus/hide releases are outside these counters. At most
16 raw IDs are retained; extra packet observations are counted as dropped while
the global Qt marker still updates. Nothing is written to an event log.

This block observes the current behavior. Button packets still supply coordinates;
there is one global Qt pointer/button state and wheel remainder, and both scroll
kinds still use ×120. No last-move authority, cursor-specific routing, trigger
freeze, click slop, drag threshold, scroll scaling or cancellation policy fix is
included. A scroll row may therefore identify cursor 1 while using cursor 0's
most recent Qt position. Last raw mouse coordinates are labelled separately from
the latest Qt position to avoid presenting scroll packets as coordinate sources.

Focusing the text field no longer opens SteamVR's keyboard. The explicit Open
SteamVR keyboard button asks the already initialized host to open it; it is
disabled in desktop presentation. The 39 on-panel keys and Done remain available.
Text lives only in the on-screen field/process memory, until reset or process
exit: no text logging, settings write or event-diagnostic payload capture.

`OverlayKeyboard` holds the existing window/target/session-token guard and UTF-8
input path. The production host binds its window only after initialization;
software scene tests supply a fake keyboard adapter directly to that same
component and never initialize a runtime or renderer graphics context. Matching
packets reach the field; stale tokens, other overlays and closed sessions do not.
Done/close and focus changes keep the existing session lifecycle.

Real-QML acceptance uses actual hit geometry for all four corners and all 39 keys
plus Done under both Y settings, scroll movement/visible position, raw/Qt numeric
observations, and two independent contexts (one hidden render-control scene and
one software window). These receipts prove software dispatch and scene behavior.
Actual SteamVR packet shapes, pointer orientation, readability and controller
feel still need a separately approved headset sitting. Shared instructions and
hardware acceptance belong to the lead.
