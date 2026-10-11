# Viewer checkpoint: opt-in WGC failure diagnostics

Source base: reviewed next `d3e739a`. Isolated branch
`codex/m2-wgc-failure-diagnostic`. Scope: theatre helper and this lane report.

Capture-only runs can explicitly request a fresh private failure directory.
Failures record parsed bounds and selection/capture-start/first-candidate/
compare-change stage, selected HWND/PID and title-match boolean, client/source/
crop dimensions, frame-pool delivery/recreation/readback counters, unchanged
spatial statistics, RGB comparison outcomes and compositor timestamps. No actual
window titles or operator/library details appear in this report.

At most two immutable selected-client samples are retained: first candidate
(first raw if no candidate exists) and latest raw. They export under distinct
failure-only names with accepted=false metadata. Fresh-directory reservation,
no linked ancestors, directory locking and exclusive file creation protect
previous artifacts. Metadata precedes image export; partial export is still a
failed diagnostic, never accepted proof. The original failure exits 1.

Existing Dolphin/PID/title/window admission, no retarget/fallback, whole-image
candidate threshold, alpha/border exclusion, one-second minimum, RGB-change gate
and accepted PNG/receipt generation are preserved. No per-eye/game/mission/
headset acceptance follows from these diagnostics.

Authored deterministic CPU coverage: stage/counters, no-frame versus recreation/
readback failure, exact spatial threshold boundary, alpha/border parity, unchanged
RGB, stalled/regressing timestamps, immutable bounded retention and independent
JSON parsing. Extended software PNG round-trip coverage checks failure exports
and exclusive-write rejection without changing existing proof images. Relative
diagnostic directories have a pre-admission rejection check.

Verified at source checkpoint: whitespace check, Python wrapper syntax and nine
privacy regressions pass. Native compilation and CTest are pending the lead's
explicit CPU slot release after calibration; authored C++ fixtures are not yet
claimed executed. No build, game/helper capture, SteamVR, headset, input takeover,
media read or system change in this lane. Runtime diagnostics and the one retry
remain unverified and require parent review/scheduling.
